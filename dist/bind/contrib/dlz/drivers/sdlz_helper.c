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

#ifdef DLZ

#include <config.h>

#include <dns/log.h>
#include <dns/result.h>

#include <isc/mem.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dlz/sdlz_helper.h>

/*
 * sdlz helper methods
 */

/*%
 * properly destroys a querylist by de-allocating the
 * memory for each query segment, and then the list itself
 */

static void
destroy_querylist(isc_mem_t *mctx, query_list_t **querylist)
{
	query_segment_t *tseg = NULL;
	query_segment_t *nseg = NULL;

	REQUIRE(mctx != NULL);

	/* if query list is null, nothing to do */
	if (*querylist == NULL)
		return;

	/* start at the top of the list */
	nseg = ISC_LIST_HEAD(**querylist);
	while (nseg != NULL) {	/* loop, until end of list */
		tseg = nseg;
		/*
		 * free the query segment's text string but only if it
		 * was really a query segment, and not a pointer to
		 * %zone%, or %record%, or %client%
		*/
		if (tseg->sql != NULL && tseg->direct == isc_boolean_true)
			isc_mem_free(mctx, tseg->sql);
		/* get the next query segment, before we destroy this one. */
		nseg = ISC_LIST_NEXT(nseg, link);
		/* deallocate this query segment. */
		isc_mem_put(mctx, tseg, sizeof(query_segment_t));
	}
	/* deallocate the query segment list */
	isc_mem_put(mctx, *querylist, sizeof(query_list_t));
}

/*% constructs a query list by parsing a string into query segments */
static isc_result_t
build_querylist(isc_mem_t *mctx, const char *query_str, char **zone,
		char **record, char **client, query_list_t **querylist,
		unsigned int flags)
{
	isc_result_t result;
	isc_boolean_t foundzone = isc_boolean_false;
	isc_boolean_t foundrecord = isc_boolean_false;
	isc_boolean_t foundclient = isc_boolean_false;
	char *temp_str = NULL;
	char *right_str = NULL;
	query_list_t *tql;
	query_segment_t *tseg = NULL;

	REQUIRE(querylist != NULL && *querylist == NULL);
	REQUIRE(mctx != NULL);

	/* if query string is null, or zero length */
	if (query_str == NULL || strlen(query_str) < 1) {
		if ((flags & SDLZH_REQUIRE_QUERY) == 0)
			/* we don't need it were ok. */
			return (ISC_R_SUCCESS);
		else
			/* we did need it, PROBLEM!!! */
			return (ISC_R_FAILURE);
	}

	/* allocate memory for query list */
	tql = isc_mem_get(mctx, sizeof(query_list_t));
	/* couldn't allocate memory.  Problem!! */
	if (tql == NULL)
		return (ISC_R_NOMEMORY);

	/* initialize the query segment list */
	ISC_LIST_INIT(*tql);

	/* make a copy of query_str so we can chop it up */
	temp_str = right_str = isc_mem_strdup(mctx, query_str);
	/* couldn't make a copy, problem!! */
	if (right_str == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* loop through the string and chop it up */
	while (right_str != NULL) {
		/* allocate memory for tseg */
		tseg = isc_mem_get(mctx, sizeof(query_segment_t));
		if (tseg  == NULL) {	/* no memory, clean everything up. */
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		tseg->sql = NULL;
		tseg->direct = isc_boolean_false;
		/* initialize the query segment link */
		ISC_LINK_INIT(tseg, link);
		/* append the query segment to the list */
		ISC_LIST_APPEND(*tql, tseg, link);

		/*
		 * split string at the first "$". set query segment to
		 * left portion
		 */
		tseg->sql = isc_mem_strdup(mctx,
					   isc_string_separate(&right_str,
							       "$"));
		if (tseg->sql == NULL) {
			/* no memory, clean everything up. */
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		/* tseg->sql points directly to a string. */
		tseg->direct = isc_boolean_true;
		tseg->strlen = strlen(tseg->sql);

		/* check if we encountered "$zone$" token */
		if (strcasecmp(tseg->sql, "zone") == 0) {
			/*
			 * we don't really need, or want the "zone"
			 * text, so get rid of it.
			 */
			isc_mem_free(mctx, tseg->sql);
			/* set tseg->sql to in-direct zone string */
			tseg->sql = (char**) zone;
			tseg->strlen = 0;
			/* tseg->sql points in-directly to a string */
			tseg->direct = isc_boolean_false;
			foundzone = isc_boolean_true;
			/* check if we encountered "$record$" token */
		} else if (strcasecmp(tseg->sql, "record") == 0) {
			/*
			 * we don't really need, or want the "record"
			 * text, so get rid of it.
			 */
			isc_mem_free(mctx, tseg->sql);
			/* set tseg->sql to in-direct record string */
			tseg->sql = (char**) record;
			tseg->strlen = 0;
			/* tseg->sql points in-directly poinsts to a string */
			tseg->direct = isc_boolean_false;
			foundrecord = isc_boolean_true;
			/* check if we encountered "$client$" token */
		} else if (strcasecmp(tseg->sql, "client") == 0) {
			/*
			 * we don't really need, or want the "client"
			 * text, so get rid of it.
			 */
			isc_mem_free(mctx, tseg->sql);
			/* set tseg->sql to in-direct record string */
			tseg->sql = (char**) client;
			tseg->strlen = 0;
			/* tseg->sql points in-directly poinsts to a string */
			tseg->direct = isc_boolean_false;
			foundclient = isc_boolean_true;
		}
	}

	/* we don't need temp_str any more */
	isc_mem_free(mctx, temp_str);
	/*
	 * add checks later to verify zone and record are found if
	 * necessary.
	 */

	/* if this query requires %client%, make sure we found it */
	if (((flags & SDLZH_REQUIRE_CLIENT) != 0) && (!foundclient) ) {
		/* Write error message to log */
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Required token $client$ not found.");
		result = ISC_R_FAILURE;
		goto flag_fail;
	}

	/* if this query requires %record%, make sure we found it */
	if (((flags & SDLZH_REQUIRE_RECORD) != 0) && (!foundrecord) ) {
		/* Write error message to log */
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Required token $record$ not found.");
		result = ISC_R_FAILURE;
		goto flag_fail;
	}

	/* if this query requires %zone%, make sure we found it */
	if (((flags & SDLZH_REQUIRE_ZONE) != 0) && (!foundzone) ) {
		/* Write error message to log */
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Required token $zone$ not found.");
		result = ISC_R_FAILURE;
		goto flag_fail;
	}

	/* pass back the query list */
	*querylist = (query_list_t *) tql;

	/* return success */
	return (ISC_R_SUCCESS);

 cleanup:
	/* get rid of temp_str */
	if (temp_str != NULL)
		isc_mem_free(mctx, temp_str);

 flag_fail:
	/* get rid of what was build of the query list */
	if (tql != NULL)
		destroy_querylist(mctx, &tql);
	return result;
}

/*%
 * build a query string from query segments, and dynamic segments
 * dynamic segments replace where the tokens %zone%, %record%, %client%
 * used to be in our queries from named.conf
 */
char *
sdlzh_build_querystring(isc_mem_t *mctx, query_list_t *querylist)
{
	query_segment_t *tseg = NULL;
	unsigned int length = 0;
	char *qs = NULL;

	REQUIRE(mctx != NULL);
	REQUIRE(querylist != NULL);

	/* start at the top of the list */
	tseg = ISC_LIST_HEAD(*querylist);
	while (tseg != NULL) {
		/*
		 * if this is a query segment, use the
		 * precalculated string length
		 */
		if (tseg->direct == isc_boolean_true)
			length += tseg->strlen;
		else	/* calculate string length for dynamic segments. */
			length += strlen(* (char**) tseg->sql);
		/* get the next segment */
		tseg = ISC_LIST_NEXT(tseg, link);
	}

	/* allocate memory for the string */
	qs = isc_mem_allocate(mctx, length + 1);
	/* couldn't allocate memory,  We need more ram! */
	if (qs == NULL)
		return NULL;

	/* start at the top of the list again */
	tseg = ISC_LIST_HEAD(*querylist);
	/* copy the first item in the list to the query string */
	if (tseg->direct == isc_boolean_true)	/* query segment */
		strcpy(qs, tseg->sql);
	else
		strcpy(qs, * (char**) tseg->sql); /* dynamic segment */

	/* concatonate the rest of the segments */
	while ((tseg = ISC_LIST_NEXT(tseg, link)) != NULL) {
		if (tseg->direct == isc_boolean_true)
			/* query segments */
			strcat(qs, tseg->sql);
		else
			/* dynamic segments */
			strcat(qs, * (char**) tseg->sql);
	}

	return qs;
}

/*% constructs a sql dbinstance (DBI) */
isc_result_t
sdlzh_build_sqldbinstance(isc_mem_t *mctx, const char *allnodes_str,
			 const char *allowxfr_str, const char *authority_str,
			 const char *findzone_str, const char *lookup_str,
			 const char *countzone_str, dbinstance_t **dbi)
{

	isc_result_t result;
	dbinstance_t *db = NULL;

	REQUIRE(dbi != NULL && *dbi == NULL);
	REQUIRE(mctx != NULL);

	/* allocate and zero memory for driver structure */
	db = isc_mem_get(mctx, sizeof(dbinstance_t));
	if (db == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not allocate memory for "
			      "database instance object.");
		return (ISC_R_NOMEMORY);
	}
	memset(db, 0, sizeof(dbinstance_t));
	db->dbconn = NULL;
	db->client = NULL;
	db->record = NULL;
	db->zone = NULL;
	db->mctx = NULL;
	db->query_buf = NULL;
	db->allnodes_q = NULL;
	db->allowxfr_q = NULL;
	db->authority_q = NULL;
	db->findzone_q = NULL;
	db->countzone_q = NULL;
	db->lookup_q = NULL;

	/* attach to the memory context */
	isc_mem_attach(mctx, &db->mctx);

	/* initialize the reference count mutex */
	result = isc_mutex_init(&db->instance_lock);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_mutex_init() failed: %s",
				 isc_result_totext(result));
		goto cleanup;
	}

	/* build the all nodes query list */
	result = build_querylist(mctx, allnodes_str, &db->zone,
				 &db->record, &db->client,
				 &db->allnodes_q, SDLZH_REQUIRE_ZONE);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not build all nodes query list");
		goto cleanup;
	}

	/* build the allow zone transfer query list */
	result = build_querylist(mctx, allowxfr_str, &db->zone,
				 &db->record, &db->client,
				 &db->allowxfr_q,
				 SDLZH_REQUIRE_ZONE | SDLZH_REQUIRE_CLIENT);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not build allow xfr query list");
		goto cleanup;
	}

	/* build the authority query, query list */
	result = build_querylist(mctx, authority_str, &db->zone,
				 &db->record, &db->client,
				 &db->authority_q, SDLZH_REQUIRE_ZONE);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not build authority query list");
		goto cleanup;
	}

	/* build findzone query, query list */
	result = build_querylist(mctx, findzone_str, &db->zone,
				 &db->record, &db->client,
				 &db->findzone_q, SDLZH_REQUIRE_ZONE);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not build find zone query list");
		goto cleanup;
	}

	/* build countzone query, query list */
	result = build_querylist(mctx, countzone_str, &db->zone,
				 &db->record, &db->client,
				 &db->countzone_q, SDLZH_REQUIRE_ZONE);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not build count zone query list");
		goto cleanup;
	}

	/* build lookup query, query list */
	result = build_querylist(mctx, lookup_str, &db->zone,
				 &db->record, &db->client,
				 &db->lookup_q, SDLZH_REQUIRE_RECORD);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not build lookup query list");
		goto cleanup;
	}

	/* pass back the db instance */
	*dbi = (dbinstance_t *) db;

	/* return success */
	return (ISC_R_SUCCESS);

 cleanup:
	/* destroy whatever was build of the db instance */
	destroy_sqldbinstance(db);
	/* return failure */
	return (ISC_R_FAILURE);
}

void
sdlzh_destroy_sqldbinstance(dbinstance_t *dbi)
{
	isc_mem_t *mctx;

	/* save mctx for later */
	mctx = dbi->mctx;

	/* destroy any query lists we created */
	destroy_querylist(mctx, &dbi->allnodes_q);
	destroy_querylist(mctx, &dbi->allowxfr_q);
	destroy_querylist(mctx, &dbi->authority_q);
	destroy_querylist(mctx, &dbi->findzone_q);
	destroy_querylist(mctx, &dbi->countzone_q);
	destroy_querylist(mctx, &dbi->lookup_q);

	/* get rid of the mutex */
	isc_mutex_destroy(&dbi->instance_lock);

	/* return, and detach the memory */
	isc_mem_put(mctx, dbi, sizeof(dbinstance_t));
	isc_mem_detach(&mctx);
}

char *
sdlzh_get_parameter_value(isc_mem_t *mctx, const char *input, const char* key)
{
	int keylen;
	char *keystart;
	char value[255];
	int i;

	if (key == NULL || input == NULL || strlen(input) < 1)
		return NULL;

	keylen = strlen(key);

	if (keylen < 1)
		return NULL;

	keystart = strstr(input, key);

	if (keystart == NULL)
		return NULL;

	REQUIRE(mctx != NULL);

	for (i = 0; i < 255; i++) {
		value[i] = keystart[keylen + i];
		if (value[i] == ' ' || value[i] == '\0') {
			value[i] = '\0';
			break;
		}
	}

	return isc_mem_strdup(mctx, value);
}

#endif
