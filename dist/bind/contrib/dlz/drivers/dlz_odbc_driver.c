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

#ifdef DLZ_ODBC

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
#include <dlz/dlz_odbc_driver.h>

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

static dns_sdlzimplementation_t *dlz_odbc = NULL;

#define dbc_search_limit 30
#define ALLNODES 1
#define ALLOWXFR 2
#define AUTHORITY 3
#define FINDZONE 4
#define LOOKUP 5

#define sqlOK(a) ((a == SQL_SUCCESS || a == SQL_SUCCESS_WITH_INFO) ? -1 : 0)

/*
 * Private Structures
 */

/*
 * structure to hold ODBC connection & statement
 */

typedef struct{
	SQLHDBC   dbc;
	SQLHSTMT  stmnt;
} odbc_db_t;

/*
 * Structure to hold everthing needed by this "instance" of the odbc driver
 * remember, the driver code is only loaded once, but may have many separate
 * instances
 */

typedef struct {

#ifdef ISC_PLATFORM_USETHREADS

    db_list_t    *db;       /* handle to a list of DB */

#else

    dbinstance_t *db;       /* handle to db */

#endif

    SQLHENV      sql_env;  /* handle to SQL environment */
    SQLCHAR      *dsn;
    SQLCHAR      *user;
    SQLCHAR      *pass;
} odbc_instance_t;

/* forward reference */

static size_t
odbc_makesafe(char *to, const char *from, size_t length);

/*
 * Private methods
 */

static SQLSMALLINT
safeLen(void *a) {
	if (a == NULL)
		return 0;
	return strlen((char *) a);
}

/*% propertly cleans up an odbc_instance_t */

static void
destroy_odbc_instance(odbc_instance_t *odbc_inst) {

#ifdef ISC_PLATFORM_USETHREADS

	dbinstance_t *ndbi = NULL;
	dbinstance_t *dbi = NULL;

	/* get the first DBI in the list */
	ndbi = ISC_LIST_HEAD(*odbc_inst->db);

	/* loop through the list */
	while (ndbi != NULL) {
		dbi = ndbi;
		/* get the next DBI in the list */
		ndbi = ISC_LIST_NEXT(dbi, link);

		/* if we have a connection / statement object in memory */
		if (dbi->dbconn != NULL) {
			/* free statement handle */
			if (((odbc_db_t *) (dbi->dbconn))->stmnt != NULL) {
				SQLFreeHandle(SQL_HANDLE_STMT,
					      ((odbc_db_t *)
					       (dbi->dbconn))->stmnt);
				((odbc_db_t *) (dbi->dbconn))->stmnt = NULL;
			}

			/* disconnect from database & free connection handle */
			if (((odbc_db_t *) (dbi->dbconn))->dbc != NULL) {
				SQLDisconnect(((odbc_db_t *)
					       dbi->dbconn)->dbc);
				SQLFreeHandle(SQL_HANDLE_DBC,
					      ((odbc_db_t *)
					       (dbi->dbconn))->dbc);
				((odbc_db_t *) (dbi->dbconn))->dbc = NULL;
			}

			/* free memory that held connection & statement. */
			isc_mem_free(ns_g_mctx, dbi->dbconn);
		}
		/* release all memory that comprised a DBI */
		destroy_sqldbinstance(dbi);
	}
	/* release memory for the list structure */
	isc_mem_put(ns_g_mctx, odbc_inst->db, sizeof(db_list_t));

#else /* ISC_PLATFORM_USETHREADS */

	/* free statement handle */
	if (((odbc_db_t *) (odbc_inst->db->dbconn))->stmnt != NULL) {
		SQLFreeHandle(SQL_HANDLE_STMT,
			      ((odbc_db_t *) (odbc_inst->db->dbconn))->stmnt);
		((odbc_db_t *) (odbc_inst->db->dbconn))->stmnt = NULL;
	}

	/* disconnect from database, free connection handle */
	if (((odbc_db_t *) (odbc_inst->db->dbconn))->dbc != NULL) {
		SQLDisconnect(((odbc_db_t *) (odbc_inst->db->dbconn))->dbc);
		SQLFreeHandle(SQL_HANDLE_DBC,
			      ((odbc_db_t *) (odbc_inst->db->dbconn))->dbc);
		((odbc_db_t *) (odbc_inst->db->dbconn))->dbc = NULL;
	}
	/*	free mem for the odbc_db_t structure held in db */
	if (((odbc_db_t *) odbc_inst->db->dbconn) != NULL) {
		isc_mem_free(ns_g_mctx, odbc_inst->db->dbconn);
		odbc_inst->db->dbconn = NULL;
	}

	if (odbc_inst->db != NULL)
		destroy_sqldbinstance(odbc_inst->db);

#endif /* ISC_PLATFORM_USETHREADS */


	/* free sql environment */
	if (odbc_inst->sql_env != NULL)
		SQLFreeHandle(SQL_HANDLE_ENV, odbc_inst->sql_env);

	/* free ODBC instance strings */
	if (odbc_inst->dsn != NULL)
		isc_mem_free(ns_g_mctx, odbc_inst->dsn);
	if (odbc_inst->pass != NULL)
		isc_mem_free(ns_g_mctx, odbc_inst->pass);
	if (odbc_inst->user != NULL)
		isc_mem_free(ns_g_mctx, odbc_inst->user);

	/* free memory for odbc_inst */
	if (odbc_inst != NULL)
		isc_mem_put(ns_g_mctx, odbc_inst, sizeof(odbc_instance_t));

}

/*% Connects to database, and creates ODBC statements */

static isc_result_t
odbc_connect(odbc_instance_t *dbi, odbc_db_t **dbc) {

	odbc_db_t *ndb = *dbc;
	SQLRETURN sqlRes;
	isc_result_t result = ISC_R_SUCCESS;

	if (ndb != NULL) {
		/*
		 * if db != null, we have to do some cleanup
		 * if statement handle != null free it
		 */
		if (ndb->stmnt != NULL) {
			SQLFreeHandle(SQL_HANDLE_STMT, ndb->stmnt);
			ndb->stmnt = NULL;
		}

		/* if connection handle != null free it */
		if (ndb->dbc != NULL) {
			SQLFreeHandle(SQL_HANDLE_DBC, ndb->dbc);
			ndb->dbc = NULL;
		}
	} else {
		ndb = isc_mem_allocate(ns_g_mctx, sizeof(odbc_db_t));
		if (ndb == NULL) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Odbc driver unable to allocate memory");
			return ISC_R_NOMEMORY;
		}
		memset(ndb, 0, sizeof(odbc_db_t));
	}

	sqlRes = SQLAllocHandle(SQL_HANDLE_DBC, dbi->sql_env, &(ndb->dbc));
	if (!sqlOK(sqlRes)) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to allocate memory");
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	sqlRes = SQLConnect(ndb->dbc, dbi->dsn, safeLen(dbi->dsn), dbi->user,
			    safeLen(dbi->user), dbi->pass, safeLen(dbi->pass));
	if (!sqlOK(sqlRes)) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to connect");
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	sqlRes = SQLAllocHandle(SQL_HANDLE_STMT, ndb->dbc, &(ndb->stmnt));
	if (!sqlOK(sqlRes)) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to allocate memory");
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	*dbc = ndb;

	return ISC_R_SUCCESS;

 cleanup:

	if (ndb != NULL) {

		/* if statement handle != null free it */
		if (ndb->stmnt != NULL) {
			SQLFreeHandle(SQL_HANDLE_STMT, ndb->stmnt);
			ndb->stmnt = NULL;
		}

		/* if connection handle != null free it */
		if (ndb->dbc != NULL) {
			SQLDisconnect(ndb->dbc);
			SQLFreeHandle(SQL_HANDLE_DBC, ndb->dbc);
			ndb->dbc = NULL;
		}
		/* free memory holding ndb */
		isc_mem_free(ns_g_mctx, ndb);
	}

	return result;
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

#ifdef ISC_PLATFORM_USETHREADS

static dbinstance_t *
odbc_find_avail_conn(db_list_t *dblist)
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
		      "Odbc driver unable to find available "
		      "connection after searching %d times",
		      count);
	return NULL;
}

#endif /* ISC_PLATFORM_USETHREADS */

/*% Allocates memory for a new string, and then constructs the new
 * string by "escaping" the input string.  The new string is
 * safe to be used in queries.  This is necessary because we cannot
 * be sure of what types of strings are passed to us, and we don't
 * want special characters in the string causing problems.
 */

static char *
odbc_escape_string(const char *instr) {

	char *outstr;
	unsigned int len;

	if (instr == NULL)
		return NULL;

	len = strlen(instr);

	outstr = isc_mem_allocate(ns_g_mctx ,(2 * len * sizeof(char)) + 1);
	if (outstr == NULL)
		return NULL;

	odbc_makesafe(outstr, instr, len);

	return outstr;
}

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
 *
 * The copyright statements from the original file containing this
 * function are included below:
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * ---------------
 */

static size_t
odbc_makesafe(char *to, const char *from, size_t length)
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
 * obtain a result set.  The data base instance that is used is returned
 * to the caller so they can get the data from the result set from it.
 * If successfull, it will be the responsibility of the caller to close
 * the cursor, and unlock the mutex of the DBI when they are done with it.
 * If not successfull, this function will perform all the cleanup.
 */


static isc_result_t
odbc_get_resultset(const char *zone, const char *record,
		   const char *client, unsigned int query,
		   void *dbdata, dbinstance_t **r_dbi)
{

	isc_result_t result;
	dbinstance_t *dbi = NULL;
	char *querystring = NULL;
	unsigned int j = 0;
	SQLRETURN sqlRes;

	REQUIRE(*r_dbi == NULL);

	/* get db instance / connection */
#ifdef ISC_PLATFORM_USETHREADS

	/* find an available DBI from the list */
	dbi = odbc_find_avail_conn(((odbc_instance_t *) dbdata)->db);

#else /* ISC_PLATFORM_USETHREADS */

	/*
	 * only 1 DBI - no need to lock instance lock either
	 * only 1 thread in the whole process, no possible contention.
	 */
	dbi =  (dbinstance_t *) ((odbc_instance_t *) dbdata)->db;

#endif /* ISC_PLATFORM_USETHREADS */

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
				 "odbc_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}


	/*
	 * was a zone string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (zone != NULL) {
		dbi->zone = odbc_escape_string(zone);
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
		dbi->record = odbc_escape_string(record);
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
		dbi->client = odbc_escape_string(client);
		if (dbi->client == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->client = NULL;
	}

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
				 "odbc_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

	/* if the querystring is null, Bummer, outta RAM.  UPGRADE TIME!!!   */
	if (querystring  == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* output the full query string during debug so we can see */
	/* what lame error the query has. */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "\nQuery String: %s\n", querystring);

	/* attempt query up to 3 times. */
	for (j=0; j < 3; j++) {
		/* try to get result set */
		sqlRes = SQLExecDirect(((odbc_db_t *) dbi->dbconn)->stmnt,
				       (SQLCHAR *) querystring,
				       (SQLINTEGER) strlen(querystring));

		/* if error, reset DB connection */
		if (!sqlOK(sqlRes)) {
			/* close cursor */
			SQLCloseCursor(((odbc_db_t *) dbi->dbconn)->stmnt);
			/* attempt to reconnect */
			result = odbc_connect((odbc_instance_t *) dbdata,
					      (odbc_db_t **) &(dbi->dbconn));
			/* check if we reconnected */
			if (result != ISC_R_SUCCESS)
				break;
			/* incase this is the last time through the loop */
			result = ISC_R_FAILURE;
		} else {
			result = ISC_R_SUCCESS;
			/* return dbi */
			*r_dbi = dbi;
			/* result set ok, break loop */
			break;
		}
	}	/* end for loop */

 cleanup:	/* it's always good to cleanup after yourself */

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

	/* if we are done using this dbi, release the lock */
	if (result != ISC_R_SUCCESS)
		isc_mutex_unlock(&dbi->instance_lock);

#endif /* ISC_PLATFORM_USETHREADS */

	/* release query string */
	if (querystring  != NULL)
		isc_mem_free(ns_g_mctx, querystring );

	/* return result */
	return result;

}

/*%
 * Gets a single field from the ODBC statement.  The memory for the
 * returned data is dynamically allocated.  If this method is successful
 * it is the reponsibility of the caller to free the memory using
 * isc_mem_free(ns_g_mctx, *ptr);
 */

static isc_result_t
odbc_getField(SQLHSTMT *stmnt, SQLSMALLINT field, char **data) {

	SQLINTEGER size;

	REQUIRE(data != NULL && *data == NULL);

	if (sqlOK(SQLColAttribute(stmnt, field, SQL_DESC_DISPLAY_SIZE,
				  NULL, 0, NULL, &size)) && size > 0) {
		*data = isc_mem_allocate(ns_g_mctx, size + 1);
		if (data != NULL) {
			if (sqlOK(SQLGetData(stmnt, field, SQL_C_CHAR,
					     *data, size + 1,&size)))
				return ISC_R_SUCCESS;
			isc_mem_free(ns_g_mctx, *data);
		}
	}
	return ISC_R_FAILURE;
}

/*%
 * Gets multiple fields from the ODBC statement.  The memory for the
 * returned data is dynamically allocated.  If this method is successful
 * it is the reponsibility of the caller to free the memory using
 * isc_mem_free(ns_g_mctx, *ptr);
 */

static isc_result_t
odbc_getManyFields(SQLHSTMT *stmnt, SQLSMALLINT startField,
		   SQLSMALLINT endField, char **retData) {

	isc_result_t result;
	SQLINTEGER size;
	int totSize = 0;
	SQLSMALLINT i;
	int j = 0;
	char *data;

	REQUIRE(retData != NULL && *retData == NULL);
	REQUIRE(startField > 0 && startField <= endField);

	/* determine how large the data is */
	for (i=startField; i <= endField; i++)
		if (sqlOK(SQLColAttribute(stmnt, i, SQL_DESC_DISPLAY_SIZE,
					  NULL, 0, NULL, &size)) && size > 0) {
			/* always allow for a " " (space) character */
			totSize += (size + 1);
			/* after the data item */
		}

	if (totSize < 1)
		return ISC_R_FAILURE;

	/* allow for a "\n" at the end of the string/ */
	data = isc_mem_allocate(ns_g_mctx, ++totSize);
	if (data == NULL)
		return ISC_R_NOMEMORY;

	result = ISC_R_FAILURE;

	/* get the data and concat all fields into a large string */
	for (i=startField; i <= endField; i++) {
		if (sqlOK(SQLGetData(stmnt, i, SQL_C_CHAR, &(data[j]),
				     totSize - j, &size))) {
			if (size > 0) {
				j += size;
				data[j++] = ' ';
				data[j] = '\0';
				result = ISC_R_SUCCESS;
			}
		} else {
			isc_mem_free(ns_g_mctx, data);
			return ISC_R_FAILURE;
		}
	}

	if (result != ISC_R_SUCCESS) {
		isc_mem_free(ns_g_mctx, data);
		return result;
	}

	*retData = data;
	return ISC_R_SUCCESS;

}

/*%
 * The processing of result sets for lookup and authority are
 * exactly the same.  So that functionality has been moved
 * into this function to minimize code.
 */

static isc_result_t
odbc_process_rs(dns_sdlzlookup_t *lookup, dbinstance_t *dbi)
{


	isc_result_t result;
	SQLSMALLINT fields;
	SQLHSTMT  *stmnt;
	char *ttl_s;
	char *type;
	char *data;
	char *endp;
	int ttl;

	REQUIRE(dbi != NULL);

	stmnt = ((odbc_db_t *) (dbi->dbconn))->stmnt;

	/* get number of columns */
	if (!sqlOK(SQLNumResultCols(stmnt, &fields))) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to process result set");
		result = ISC_R_FAILURE;
		goto process_rs_cleanup;
	}

	/* get things ready for processing */
	result = ISC_R_FAILURE;

	while (sqlOK(SQLFetch(stmnt))) {

		/* set to null for next pass through */
		data = type = ttl_s = NULL;

		switch(fields) {
		case 1:
			/*
			 * one column in rs, it's the data field.  use
			 * default type of A record, and default TTL
			 * of 86400.  attempt to get data, & tell bind
			 * about it.
			 */
			if ((result = odbc_getField(stmnt, 1,
						    &data)) == ISC_R_SUCCESS) {
				result = dns_sdlz_putrr(lookup, "a",
							86400, data);
			}
			break;
		case 2:
			/*
			 * two columns, data field, and data type.
			 * use default TTL of 86400.  attempt to get
			 * DNS type & data, then tell bind about it.
			 */
			if ((result = odbc_getField(stmnt, 1,
						    &type)) == ISC_R_SUCCESS &&
			    (result = odbc_getField(stmnt, 2,
						    &data)) == ISC_R_SUCCESS) {
				result = dns_sdlz_putrr(lookup, type,
							86400, data);
			}
			break;
		default:
			/*
			 * 3 fields or more, concatenate the last ones
			 * together.  attempt to get DNS ttl, type,
			 * data then tell Bind about them.
			 */
			if ((result = odbc_getField(stmnt, 1, &ttl_s))
				== ISC_R_SUCCESS &&
			    (result = odbc_getField(stmnt, 2, &type))
				== ISC_R_SUCCESS &&
			    (result = odbc_getManyFields(stmnt, 3,
							 fields, &data))
				== ISC_R_SUCCESS) {
				/* try to convert ttl string to int */
				ttl = strtol(ttl_s, &endp, 10);
				/* failure converting ttl. */
				if (*endp != '\0' || ttl < 0) {
					isc_log_write(dns_lctx,
						      DNS_LOGCATEGORY_DATABASE,
						      DNS_LOGMODULE_DLZ,
						      ISC_LOG_ERROR,
						      "Odbc driver ttl must "
						      "be a postive number");
					result = ISC_R_FAILURE;
				} else {
					/*
					 * successful converting TTL,
					 * tell Bind everything
					 */
					result = dns_sdlz_putrr(lookup, type,
								ttl, data);
				}
			} /* closes bid if () */
		} /* closes switch(fields) */

		/* clean up mem */
		if (ttl_s != NULL)
			isc_mem_free(ns_g_mctx, ttl_s);
		if (type != NULL)
			isc_mem_free(ns_g_mctx, type);
		if (data != NULL)
			isc_mem_free(ns_g_mctx, data);

		/* I sure hope we were successful */
		if (result != ISC_R_SUCCESS) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "dns_sdlz_putrr returned error. "
				      "Error code was: %s",
				      isc_result_totext(result));
			result = ISC_R_FAILURE;
			goto process_rs_cleanup;
		}
	} /* closes while loop */

 process_rs_cleanup:

	/* close cursor */
	SQLCloseCursor(((odbc_db_t *) (dbi->dbconn))->stmnt);

#ifdef ISC_PLATFORM_USETHREADS

	/* free lock on dbi so someone else can use it. */
	isc_mutex_unlock(&dbi->instance_lock);

#endif

	return result;
}

/*
 * SDLZ interface methods
 */

/*% determine if the zone is supported by (in) the database */

static isc_result_t
odbc_findzone(void *driverarg, void *dbdata, const char *name)
{

	isc_result_t result;
	dbinstance_t *dbi = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	/* if result != ISC_R_SUCCESS cursor and mutex already cleaned up. */
	/* so we don't have to do it here. */
	result = odbc_get_resultset(name, NULL, NULL, FINDZONE, dbdata, &dbi);

	/* Check that we got a result set with data */
	if (result == ISC_R_SUCCESS &&
	    !sqlOK(SQLFetch(((odbc_db_t *) (dbi->dbconn))->stmnt))) {
		result = ISC_R_NOTFOUND;
	}

	if (dbi != NULL) {
		/* get rid of result set, we are done with it. */
		SQLCloseCursor(((odbc_db_t *) (dbi->dbconn))->stmnt);

#ifdef ISC_PLATFORM_USETHREADS

		/* free lock on dbi so someone else can use it. */
		isc_mutex_unlock(&dbi->instance_lock);
#endif
	}

	return result;
}

/*% Determine if the client is allowed to perform a zone transfer */
static isc_result_t
odbc_allowzonexfr(void *driverarg, void *dbdata, const char *name,
		  const char *client)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;

	UNUSED(driverarg);

	/* first check if the zone is supported by the database. */
	result = odbc_findzone(driverarg, dbdata, name);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_NOTFOUND);

	/*
	 * if we get to this point we know the zone is supported by
	 * the database.  the only questions now are is the zone
	 * transfer is allowed for this client and did the config file
	 * have an allow zone xfr query
	 *
	 * Run our query, and get a result set from the database.  if
	 * result != ISC_R_SUCCESS cursor and mutex already cleaned
	 * up, so we don't have to do it here.
	 */
	result = odbc_get_resultset(name, NULL, client, ALLOWXFR,
				    dbdata, &dbi);

	/* if we get "not implemented", send it along. */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;

	/* Check that we got a result set with data */
	if (result == ISC_R_SUCCESS &&
	    !sqlOK(SQLFetch(((odbc_db_t *) (dbi->dbconn))->stmnt))) {
		result = ISC_R_NOPERM;
	}

	if (dbi != NULL) {
		/* get rid of result set, we are done with it. */
		SQLCloseCursor(((odbc_db_t *) (dbi->dbconn))->stmnt);

#ifdef ISC_PLATFORM_USETHREADS

		/* free lock on dbi so someone else can use it. */
		isc_mutex_unlock(&dbi->instance_lock);
#endif

	}

	return result;
}

/*%
 * If the client is allowed to perform a zone transfer, the next order of
 * business is to get all the nodes in the zone, so bind can respond to the
 * query.
 */

static isc_result_t
odbc_allnodes(const char *zone, void *driverarg, void *dbdata,
	      dns_sdlzallnodes_t *allnodes)
{

	isc_result_t result;
	dbinstance_t *dbi = NULL;
	SQLHSTMT  *stmnt;
	SQLSMALLINT fields;
	char *data;
	char *type;
	char *ttl_s;
	int ttl;
	char *host;
	char *endp;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = odbc_get_resultset(zone, NULL, NULL, ALLNODES, dbdata, &dbi);

	/* if we get "not implemented", send it along */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;

	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to return "
			      "result set for all nodes query");
		return (ISC_R_FAILURE);
	}

	stmnt = ((odbc_db_t *) (dbi->dbconn))->stmnt;

	/* get number of columns */
	if (!sqlOK(SQLNumResultCols(stmnt, &fields))) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to process result set");
		result = ISC_R_FAILURE;
		goto allnodes_cleanup;
	}

	if (fields < 4) {	/* gotta have at least 4 columns */
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver too few fields returned by "
			      "all nodes query");
		result = ISC_R_FAILURE;
		goto allnodes_cleanup;
	}

	/* get things ready for processing */
	result = ISC_R_FAILURE;

	while (sqlOK(SQLFetch(stmnt))) {

		/* set to null for next pass through */
		data = host = type = ttl_s = NULL;

		/*
		 * attempt to get DNS ttl, type, host, data then tell
		 * Bind about them
		 */
		if ((result = odbc_getField(stmnt, 1,
					    &ttl_s)) == ISC_R_SUCCESS &&
		    (result = odbc_getField(stmnt, 2,
					    &type)) == ISC_R_SUCCESS &&
		    (result = odbc_getField(stmnt, 3,
					    &host)) == ISC_R_SUCCESS &&
		    (result = odbc_getManyFields(stmnt, 4, fields,
						 &data)) == ISC_R_SUCCESS) {
			/* convert ttl string to int */
			ttl = strtol(ttl_s, &endp, 10);
			/* failure converting ttl. */
			if (*endp != '\0' || ttl < 0) {
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "Odbc driver ttl must be "
					      "a postive number");
				result = ISC_R_FAILURE;
			} else {
				/* successful converting TTL, tell Bind  */
				result = dns_sdlz_putnamedrr(allnodes, host,
							     type, ttl, data);
			}
		} /* closes big if () */

		/* clean up mem */
		if (ttl_s != NULL)
			isc_mem_free(ns_g_mctx, ttl_s);
		if (type != NULL)
			isc_mem_free(ns_g_mctx, type);
		if (host != NULL)
			isc_mem_free(ns_g_mctx, host);
		if (data != NULL)
			isc_mem_free(ns_g_mctx, data);

		/* if we weren't successful, log err msg */
		if (result != ISC_R_SUCCESS) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "dns_sdlz_putnamedrr returned error. "
				      "Error code was: %s",
				      isc_result_totext(result));
			result = ISC_R_FAILURE;
			goto allnodes_cleanup;
		}
	} /* closes while loop */

 allnodes_cleanup:

	/* close cursor */
	SQLCloseCursor(((odbc_db_t *) (dbi->dbconn))->stmnt);

#ifdef ISC_PLATFORM_USETHREADS

	/* free lock on dbi so someone else can use it. */
	isc_mutex_unlock(&dbi->instance_lock);

#endif

	return result;
}

/*%
 * if the lookup function does not return SOA or NS records for the zone,
 * use this function to get that information for Bind.
 */

static isc_result_t
odbc_authority(const char *zone, void *driverarg, void *dbdata,
	       dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = odbc_get_resultset(zone, NULL, NULL, AUTHORITY, dbdata, &dbi);
	/* if we get "not implemented", send it along */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to return "
			      "result set for authority query");
		return (ISC_R_FAILURE);
	}
	/* lookup and authority result sets are processed in the same manner */
	/* odbc_process_rs does the job for both functions. */
	return odbc_process_rs(lookup, dbi);
}

/*% if zone is supported, lookup up a (or multiple) record(s) in it */

static isc_result_t
odbc_lookup(const char *zone, const char *name, void *driverarg,
	    void *dbdata, dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = odbc_get_resultset(zone, name, NULL, LOOKUP, dbdata, &dbi);
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver unable to return "
			      "result set for lookup query");
		return (ISC_R_FAILURE);
	}
	/* lookup and authority result sets are processed in the same manner */
	/* odbc_process_rs does the job for both functions. */
	return odbc_process_rs(lookup, dbi);
}

/*%
 * create an instance of the driver.  Remember, only 1 copy of the driver's
 * code is ever loaded, the driver has to remember which context it's
 * operating in.  This is done via use of the dbdata argument which is
 * passed into all query functions.
 */
static isc_result_t
odbc_create(const char *dlzname, unsigned int argc, char *argv[],
	    void *driverarg, void **dbdata)
{
	isc_result_t result;
	odbc_instance_t *odbc_inst = NULL;
	dbinstance_t *db = NULL;
	SQLRETURN sqlRes;

#ifdef ISC_PLATFORM_USETHREADS
	/* if multi-threaded, we need a few extra variables. */
	int dbcount;
	int i;
	char *endp;

#endif /* ISC_PLATFORM_USETHREADS */

	UNUSED(dlzname);
	UNUSED(driverarg);

#ifdef ISC_PLATFORM_USETHREADS
	/* if debugging, let user know we are multithreaded. */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "Odbc driver running multithreaded");
#else /* ISC_PLATFORM_USETHREADS */
	/* if debugging, let user know we are single threaded. */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "Odbc driver running single threaded");
#endif /* ISC_PLATFORM_USETHREADS */

	/* verify we have at least 5 arg's passed to the driver */
	if (argc < 5) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver requires at least "
			      "4 command line args.");
		return (ISC_R_FAILURE);
	}

	/* no more than 8 arg's should be passed to the driver */
	if (argc > 8) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Odbc driver cannot accept more than "
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
			      "Odbc driver database connection count "
			      "must be positive.");
		return (ISC_R_FAILURE);
	}

#endif /* ISC_PLATFORM_USETHREADS */

	/* allocate memory for odbc instance */
	odbc_inst = isc_mem_get(ns_g_mctx, sizeof(odbc_instance_t));
	if (odbc_inst == NULL)
		return (ISC_R_NOMEMORY);
	memset(odbc_inst, 0, sizeof(odbc_instance_t));

	/* parse connection string and get paramters. */

	/* get odbc database dsn - required */
	odbc_inst->dsn = (SQLCHAR *) getParameterValue(argv[2],
						       "dsn=");
	if (odbc_inst->dsn == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "odbc driver requires a dns parameter.");
		result = ISC_R_FAILURE;
		goto cleanup;
	}
	/* get odbc database username */
	/* if no username was passed, set odbc_inst.user = NULL; */
	odbc_inst->user = (SQLCHAR *) getParameterValue(argv[2],
							"user=");

	/* get odbc database password */
	/* if no password was passed, set odbc_inst.pass = NULL; */
	odbc_inst->pass = (SQLCHAR *) getParameterValue(argv[2], "pass=");

	/* create odbc environment & set environment to ODBC V3 */
	if (odbc_inst->sql_env == NULL) {
		/* create environment handle */
		sqlRes = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE,
					&(odbc_inst->sql_env));
		if (!sqlOK(sqlRes)) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_INFO,
				      "Odbc driver unable to allocate memory");
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		/*set ODBC version = 3 */
		sqlRes = SQLSetEnvAttr(odbc_inst->sql_env,
				       SQL_ATTR_ODBC_VERSION,
				       (void *) SQL_OV_ODBC3, 0);
		if (!sqlOK(sqlRes)) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_INFO,
				      "Unable to configure ODBC environment");
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	}

#ifdef ISC_PLATFORM_USETHREADS

	/* allocate memory for database connection list */
	odbc_inst->db = isc_mem_get(ns_g_mctx, sizeof(db_list_t));
	if (odbc_inst->db == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}


	/* initialize DB connection list */
	ISC_LIST_INIT(*odbc_inst->db);

	/* create the appropriate number of database instances (DBI) */
	/* append each new DBI to the end of the list */
	for (i=0; i < dbcount; i++) {

#endif /* ISC_PLATFORM_USETHREADS */

		/* how many queries were passed in from config file? */
		switch(argc) {
		case 5:
			result = build_sqldbinstance(ns_g_mctx, NULL, NULL,
						     NULL, argv[3], argv[4],
						     NULL, &db);
			break;
		case 6:
			result = build_sqldbinstance(ns_g_mctx, NULL, NULL,
						     argv[5], argv[3], argv[4],
						     NULL, &db);
			break;
		case 7:
			result = build_sqldbinstance(ns_g_mctx, argv[6], NULL,
						     argv[5], argv[3], argv[4],
						     NULL, &db);
			break;
		case 8:
			result = build_sqldbinstance(ns_g_mctx, argv[6],
						     argv[7], argv[5], argv[3],
						     argv[4], NULL, &db);
			break;
		default:
			/* not really needed, should shut up compiler. */
			result = ISC_R_FAILURE;
		}

		/* unsuccessful?, log err msg and cleanup. */
		if (result != ISC_R_SUCCESS) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Odbc driver could not create "
				      "database instance object.");
			goto cleanup;
		}

#ifdef ISC_PLATFORM_USETHREADS

		/* when multithreaded, build a list of DBI's */
		ISC_LINK_INIT(db, link);
		ISC_LIST_APPEND(*odbc_inst->db, db, link);

#endif

		result = odbc_connect(odbc_inst, (odbc_db_t **) &(db->dbconn));

		if (result != ISC_R_SUCCESS) {

#ifdef ISC_PLATFORM_USETHREADS

			/*
			 * if multi threaded, let user know which
			 * connection failed.  user could be
			 * attempting to create 10 db connections and
			 * for some reason the db backend only allows
			 * 9.
			 */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Odbc driver failed to create database "
				      "connection number %u after 3 attempts",
				      i+1);
#else
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Odbc driver failed to create database "
				      "connection after 3 attempts");
#endif
			goto cleanup;
		}

#ifdef ISC_PLATFORM_USETHREADS

		/* set DB = null for next loop through. */
		db = NULL;

	}	/* end for loop */

#else
	/* tell odbc_inst about the db connection we just created. */
	odbc_inst->db = db;

#endif

	/* set dbdata to the odbc_instance we created. */
	*dbdata = odbc_inst;

	/* hey, we got through all of that ok, return success. */
	return(ISC_R_SUCCESS);

 cleanup:

	destroy_odbc_instance(odbc_inst);

	return result;
}

/*%
 * destroy an instance of the driver.  Remember, only 1 copy of the driver's
 * code is ever loaded, the driver has to remember which context it's
 * operating in.  This is done via use of the dbdata argument.
 * so we really only need to clean it up since we are not using driverarg.
 */

static void
odbc_destroy(void *driverarg, void *dbdata)
{
	UNUSED(driverarg);

	destroy_odbc_instance((odbc_instance_t *) dbdata);
}


/* pointers to all our runtime methods. */
/* this is used during driver registration */
/* i.e. in dlz_odbc_init below. */
static dns_sdlzmethods_t dlz_odbc_methods = {
	odbc_create,
	odbc_destroy,
	odbc_findzone,
	odbc_lookup,
	odbc_authority,
	odbc_allnodes,
	odbc_allowzonexfr,
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
dlz_odbc_init(void) {
	isc_result_t result;

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Registering DLZ odbc driver.");

	/*
	 * Driver is always threadsafe.  When multithreaded all
	 * functions use multithreaded code.  When not multithreaded,
	 * all functions can only be entered once, but only 1 thread
	 * of operation is available in Bind.  So everything is still
	 * threadsafe.
	 */
	result = dns_sdlzregister("odbc", &dlz_odbc_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_RELATIVERDATA |
				  DNS_SDLZFLAG_THREADSAFE,
				  ns_g_mctx, &dlz_odbc);
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
dlz_odbc_clear(void) {

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Unregistering DLZ odbc driver.");

	/* unregister the driver. */
	if (dlz_odbc != NULL)
		dns_sdlzunregister(&dlz_odbc);
}

#endif
