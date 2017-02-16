/*	$NetBSD: dlz_dbi.c,v 1.1.1.4 2014/12/10 03:34:31 christos Exp $	*/

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
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include <sys/errno.h>

#include <dlz_minimal.h>
#include <dlz_list.h>
#include <dlz_dbi.h>
#include <dlz_pthread.h>

/*%
 * properly destroys a querylist by de-allocating the
 * memory for each query segment, and then the list itself
 */

void
destroy_querylist(query_list_t **querylist) {
	query_segment_t *tseg = NULL;
	query_segment_t *nseg = NULL;

	/* if query list is null, nothing to do */
	if (*querylist == NULL)
		return;

	/* start at the top of the list */
	nseg = DLZ_LIST_HEAD(**querylist);
	while (nseg != NULL) {	/* loop, until end of list */
		tseg = nseg;
		/*
		 * free the query segment's text string but only if it
		 * was really a query segment, and not a pointer to
		 * %zone%, or %record%, or %client%
		*/
		if (tseg->cmd != NULL && tseg->direct == ISC_TRUE)
			free(tseg->cmd);
		/* get the next query segment, before we destroy this one. */
		nseg = DLZ_LIST_NEXT(nseg, link);
		/* deallocate this query segment. */
		free(tseg);
	}
	/* deallocate the query segment list */
	free(*querylist);
}

/*% constructs a query list by parsing a string into query segments */
isc_result_t
build_querylist(const char *query_str, char **zone, char **record,
		char **client, query_list_t **querylist, unsigned int flags, 
		log_t log)
{
	isc_result_t result;
	isc_boolean_t foundzone = ISC_FALSE;
	isc_boolean_t foundrecord = ISC_FALSE;
	isc_boolean_t foundclient = ISC_FALSE;
	char *temp_str = NULL;
	char *right_str = NULL;
	query_list_t *tql;
	query_segment_t *tseg = NULL;

	/* if query string is null, or zero length */
	if (query_str == NULL || strlen(query_str) < 1) {
		if ((flags & REQUIRE_QUERY) == 0)
			/* we don't need it were ok. */
			return (ISC_R_SUCCESS);
		else
			/* we did need it, PROBLEM!!! */
			return (ISC_R_FAILURE);
	}

	/* allocate memory for query list */
	tql = calloc(1, sizeof(query_list_t));
	/* couldn't allocate memory.  Problem!! */
	if (tql == NULL)
		return (ISC_R_NOMEMORY);

	/* initialize the query segment list */
	DLZ_LIST_INIT(*tql);

	/* make a copy of query_str so we can chop it up */
	temp_str = right_str = strdup(query_str);
	/* couldn't make a copy, problem!! */
	if (right_str == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* loop through the string and chop it up */
	while (right_str != NULL) {
		/* allocate memory for tseg */
		tseg = calloc(1, sizeof(query_segment_t));
		if (tseg  == NULL) {	/* no memory, clean everything up. */
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		tseg->cmd = NULL;
		tseg->direct = ISC_FALSE;
		/* initialize the query segment link */
		DLZ_LINK_INIT(tseg, link);
		/* append the query segment to the list */
		DLZ_LIST_APPEND(*tql, tseg, link);

		/*
		 * split string at the first "$". set query segment to
		 * left portion
		 */
		tseg->cmd = strdup(strsep(&right_str, "$"));
		if (tseg->cmd == NULL) {
			/* no memory, clean everything up. */
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		/* tseg->cmd points directly to a string. */
		tseg->direct = ISC_TRUE;
		tseg->strlen = strlen(tseg->cmd);

		/* check if we encountered "$zone$" token */
		if (strcasecmp(tseg->cmd, "zone") == 0) {
			/*
			 * we don't really need, or want the "zone"
			 * text, so get rid of it.
			 */
			free(tseg->cmd);
			/* set tseg->cmd to in-direct zone string */
			tseg->cmd = (char**) zone;
			tseg->strlen = 0;
			/* tseg->cmd points in-directly to a string */
			tseg->direct = ISC_FALSE;
			foundzone = ISC_TRUE;
			/* check if we encountered "$record$" token */
		} else if (strcasecmp(tseg->cmd, "record") == 0) {
			/*
			 * we don't really need, or want the "record"
			 * text, so get rid of it.
			 */
			free(tseg->cmd);
			/* set tseg->cmd to in-direct record string */
			tseg->cmd = (char**) record;
			tseg->strlen = 0;
			/* tseg->cmd points in-directly poinsts to a string */
			tseg->direct = ISC_FALSE;
			foundrecord = ISC_TRUE;
			/* check if we encountered "$client$" token */
		} else if (strcasecmp(tseg->cmd, "client") == 0) {
			/*
			 * we don't really need, or want the "client"
			 * text, so get rid of it.
			 */
			free(tseg->cmd);
			/* set tseg->cmd to in-direct record string */
			tseg->cmd = (char**) client;
			tseg->strlen = 0;
			/* tseg->cmd points in-directly poinsts to a string */
			tseg->direct = ISC_FALSE;
			foundclient = ISC_TRUE;
		}
	}

	/* we don't need temp_str any more */
	free(temp_str);
	/*
	 * add checks later to verify zone and record are found if
	 * necessary.
	 */

	/* if this query requires %client%, make sure we found it */
	if (((flags & REQUIRE_CLIENT) != 0) && (!foundclient) ) {
		/* Write error message to log */
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Required token $client$ not found.");
		result = ISC_R_FAILURE;
		goto flag_fail;
	}

	/* if this query requires %record%, make sure we found it */
	if (((flags & REQUIRE_RECORD) != 0) && (!foundrecord) ) {
		/* Write error message to log */
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Required token $record$ not found.");
		result = ISC_R_FAILURE;
		goto flag_fail;
	}

	/* if this query requires %zone%, make sure we found it */
	if (((flags & REQUIRE_ZONE) != 0) && (!foundzone) ) {
		/* Write error message to log */
		if (log != NULL)
			log(ISC_LOG_ERROR, "Required token $zone$ not found.");
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
		free(temp_str);

 flag_fail:
	/* get rid of what was build of the query list */
	if (tql != NULL)
		destroy_querylist(&tql);
	return (result);
}

/*%
 * build a query string from query segments, and dynamic segments
 * dynamic segments replace where the tokens %zone%, %record%, %client%
 * used to be in our queries from named.conf
 */
char *
build_querystring(query_list_t *querylist) {
	query_segment_t *tseg = NULL;
	unsigned int length = 0;
	char *qs = NULL;

	/* start at the top of the list */
	tseg = DLZ_LIST_HEAD(*querylist);
	while (tseg != NULL) {
		/*
		 * if this is a query segment, use the
		 * precalculated string length
		 */
		if (tseg->direct == ISC_TRUE)
			length += tseg->strlen;
		else	/* calculate string length for dynamic segments. */
			length += strlen(* (char**) tseg->cmd);
		/* get the next segment */
		tseg = DLZ_LIST_NEXT(tseg, link);
	}

	qs = malloc(length + 1);
	if (qs == NULL)
		return (NULL);

	*qs = '\0';
	/* start at the top of the list again */
	tseg = DLZ_LIST_HEAD(*querylist);
	while (tseg != NULL) {
		if (tseg->direct == ISC_TRUE)
			/* query segments */
			strcat(qs, tseg->cmd);
		else
			/* dynamic segments */
			strcat(qs, * (char**) tseg->cmd);
		/* get the next segment */
		tseg = DLZ_LIST_NEXT(tseg, link);
	}

	return (qs);
}

/*% constructs a dbinstance (DBI) */
isc_result_t
build_dbinstance(const char *allnodes_str, const char *allowxfr_str,
		 const char *authority_str, const char *findzone_str,
		 const char *lookup_str, const char *countzone_str,
		 dbinstance_t **dbi, log_t log)
{

	isc_result_t result;
	dbinstance_t *db = NULL;
	int err;

	/* allocate and zero memory for driver structure */
	db = calloc(1, sizeof(dbinstance_t));
	if (db == NULL) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not allocate memory for "
			    "database instance object.");
		return (ISC_R_NOMEMORY);
	}
	memset(db, 0, sizeof(dbinstance_t));
	db->dbconn = NULL;
	db->client = NULL;
	db->record = NULL;
	db->zone = NULL;
	db->query_buf = NULL;
	db->allnodes_q = NULL;
	db->allowxfr_q = NULL;
	db->authority_q = NULL;
	db->findzone_q = NULL;
	db->countzone_q = NULL;
	db->lookup_q = NULL;

	/* initialize the reference count mutex */
	err = dlz_mutex_init(&db->lock, NULL);
	if (err == ENOMEM) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	} else if (err != 0) {
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

	/* build the all nodes query list */
	result = build_querylist(allnodes_str, &db->zone, &db->record,
				 &db->client, &db->allnodes_q,
				 REQUIRE_ZONE, log);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not build all nodes query list");
		goto cleanup;
	}

	/* build the allow zone transfer query list */
	result = build_querylist(allowxfr_str, &db->zone, &db->record,
				 &db->client, &db->allowxfr_q,
				 REQUIRE_ZONE | REQUIRE_CLIENT,
				 log);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not build allow xfr query list");
		goto cleanup;
	}

	/* build the authority query, query list */
	result = build_querylist(authority_str, &db->zone, &db->record,
				 &db->client, &db->authority_q,
				 REQUIRE_ZONE, log);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not build authority query list");
		goto cleanup;
	}

	/* build findzone query, query list */
	result = build_querylist(findzone_str, &db->zone, &db->record,
				 &db->client, &db->findzone_q,
				 REQUIRE_ZONE, log);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not build find zone query list");
		goto cleanup;
	}

	/* build countzone query, query list */
	result = build_querylist(countzone_str, &db->zone, &db->record,
				 &db->client, &db->countzone_q,
				 REQUIRE_ZONE, log);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not build count zone query list");
		goto cleanup;
	}

	/* build lookup query, query list */
	result = build_querylist(lookup_str, &db->zone, &db->record,
				 &db->client, &db->lookup_q,
				 REQUIRE_RECORD, log);
	/* if unsuccessful, log err msg and cleanup */
	if (result != ISC_R_SUCCESS) {
		if (log != NULL)
			log(ISC_LOG_ERROR,
			    "Could not build lookup query list");
		goto cleanup;
	}

	/* pass back the db instance */
	*dbi = (dbinstance_t *) db;

	/* return success */
	return (ISC_R_SUCCESS);

 cleanup:
	/* destroy whatever was build of the db instance */
	destroy_dbinstance(db);
	/* return failure */
	return (ISC_R_FAILURE);
}

void
destroy_dbinstance(dbinstance_t *dbi) {
	/* destroy any query lists we created */
	destroy_querylist(&dbi->allnodes_q);
	destroy_querylist(&dbi->allowxfr_q);
	destroy_querylist(&dbi->authority_q);
	destroy_querylist(&dbi->findzone_q);
	destroy_querylist(&dbi->countzone_q);
	destroy_querylist(&dbi->lookup_q);

	/* get rid of the mutex */
	(void) dlz_mutex_destroy(&dbi->lock);

	/* return, and detach the memory */
	free(dbi);
}

char *
get_parameter_value(const char *input, const char* key) {
	int keylen;
	char *keystart;
	char value[255];
	int i;

	if (key == NULL || input == NULL || *input == '\0')
		return (NULL);

	keylen = strlen(key);

	if (keylen < 1)
		return (NULL);

	keystart = strstr(input, key);

	if (keystart == NULL)
		return (NULL);

	for (i = 0; i < 255; i++) {
		value[i] = keystart[keylen + i];
		if (isspace(value[i]) || value[i] == '\0') {
			value[i] = '\0';
			break;
		}
	}

	return (strdup(value));
}
