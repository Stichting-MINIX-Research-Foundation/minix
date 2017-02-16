/*	$NetBSD: dlz_ldap_dynamic.c,v 1.1.1.3 2014/12/10 03:34:31 christos Exp $	*/

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

/*
 * This provides the externally loadable ldap DLZ module, without
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

/*
 * Need older API functions from ldap.h.
 */
#define LDAP_DEPRECATED 1

#include <ldap.h>

#define SIMPLE "simple"
#define KRB41 "krb41"
#define KRB42 "krb42"
#define V2 "v2"
#define V3 "v3"

#define dbc_search_limit 30
#define ALLNODES 1
#define ALLOWXFR 2
#define AUTHORITY 3
#define FINDZONE 4
#define LOOKUP 5

/*%
 * Structure to hold everthing needed by this "instance" of the LDAP
 * driver remember, the driver code is only loaded once, but may have
 * many separate instances.
 */
typedef struct {
#if PTHREADS
	db_list_t    *db; /*%< handle to a list of DB */
#else
	dbinstance_t *db; /*%< handle to db */
#endif
	int method;	/*%< security authentication method */
	char *user;	/*%< who is authenticating */
	char *cred;	/*%< password for simple authentication method */
	int protocol;	/*%< LDAP communication protocol version */
	char *hosts;	/*%< LDAP server hosts */

	/* Helper functions from the dlz_dlopen driver */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;
} ldap_instance_t;

/* forward references */

#if DLZ_DLOPEN_VERSION < 3
isc_result_t
dlz_findzonedb(void *dbdata, const char *name);
#else
isc_result_t
dlz_findzonedb(void *dbdata, const char *name,
		 dns_clientinfomethods_t *methods,
		 dns_clientinfo_t *clientinfo);
#endif

void
dlz_destroy(void *dbdata);

static void
b9_add_helper(ldap_instance_t *db, const char *helper_name, void *ptr);

/*
 * Private methods
 */

/*% checks that the LDAP URL parameters make sense */
static isc_result_t
ldap_checkURL(ldap_instance_t *db, char *URL, int attrCnt, const char *msg) {
	isc_result_t result = ISC_R_SUCCESS;
	int ldap_result;
	LDAPURLDesc *ldap_url = NULL;

	if (!ldap_is_ldap_url(URL)) {
		db->log(ISC_LOG_ERROR,
			"%s query is not a valid LDAP URL", msg);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	ldap_result = ldap_url_parse(URL, &ldap_url);
	if (ldap_result != LDAP_SUCCESS || ldap_url == NULL) {
		db->log(ISC_LOG_ERROR, "parsing %s query failed", msg);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (ldap_count_values(ldap_url->lud_attrs) < attrCnt) {
		db->log(ISC_LOG_ERROR,
			"%s query must specify at least "
			"%d attributes to return", msg, attrCnt);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (ldap_url->lud_host != NULL) {
		db->log(ISC_LOG_ERROR,
			"%s query must not specify a host", msg);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (ldap_url->lud_port != 389) {
		db->log(ISC_LOG_ERROR,
			"%s query must not specify a port", msg);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (ldap_url->lud_dn == NULL || strlen (ldap_url->lud_dn) < 1) {
		db->log(ISC_LOG_ERROR,
			"%s query must specify a search base", msg);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (ldap_url->lud_exts != NULL || ldap_url->lud_crit_exts != 0) {
		db->log(ISC_LOG_ERROR,
			"%s uses extensions. "
			"The driver does not support LDAP extensions.", msg);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

 cleanup:
	if (ldap_url != NULL)
		ldap_free_urldesc(ldap_url);

	return (result);
}

/*% Connects / reconnects to LDAP server */
static isc_result_t
ldap_connect(ldap_instance_t *dbi, dbinstance_t *dbc) {
	isc_result_t result;
	int ldap_result;

	/* if we have a connection, get ride of it. */
	if (dbc->dbconn != NULL) {
		ldap_unbind_s((LDAP *) dbc->dbconn);
		dbc->dbconn = NULL;
	}

	/* now connect / reconnect. */

	/* initialize. */
	dbc->dbconn = ldap_init(dbi->hosts, LDAP_PORT);
	if (dbc->dbconn == NULL)
		return (ISC_R_NOMEMORY);

	/* set protocol version. */
	ldap_result = ldap_set_option((LDAP *) dbc->dbconn,
				      LDAP_OPT_PROTOCOL_VERSION,
				      &(dbi->protocol));
	if (ldap_result != LDAP_SUCCESS) {
		result = ISC_R_NOPERM;
		goto cleanup;
	}

	/* "bind" to server.  i.e. send username / pass */
	ldap_result = ldap_bind_s((LDAP *) dbc->dbconn, dbi->user,
				  dbi->cred, dbi->method);
	if (ldap_result != LDAP_SUCCESS) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	return (ISC_R_SUCCESS);

 cleanup:

	/* cleanup if failure. */
	if (dbc->dbconn != NULL) {
		ldap_unbind_s((LDAP *) dbc->dbconn);
		dbc->dbconn = NULL;
	}

	return (result);
}

#if PTHREADS
/*%
 * Properly cleans up a list of database instances.
 * This function is only used when the driver is compiled for
 * multithreaded operation.
 */
static void
ldap_destroy_dblist(db_list_t *dblist) {
	dbinstance_t *ndbi = NULL;
	dbinstance_t *dbi = NULL;

	/* get the first DBI in the list */
	ndbi = DLZ_LIST_HEAD(*dblist);

	/* loop through the list */
	while (ndbi != NULL) {
		dbi = ndbi;
		/* get the next DBI in the list */
		ndbi = DLZ_LIST_NEXT(dbi, link);
		/* release DB connection */
		if (dbi->dbconn != NULL)
			ldap_unbind_s((LDAP *) dbi->dbconn);
		/* release all memory that comprised a DBI */
		destroy_dbinstance(dbi);
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
 * This function is only used when the driver is compiled for
 * multithreaded operation.
 */
static dbinstance_t *
ldap_find_avail_conn(ldap_instance_t *ldap) {
	dbinstance_t *dbi = NULL;
	dbinstance_t *head;
	int count = 0;

	/* get top of list */
	head = dbi = DLZ_LIST_HEAD(*ldap->db);

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

	ldap->log(ISC_LOG_INFO,
		"LDAP driver unable to find available connection "
		"after searching %d times", count);
	return (NULL);
}
#endif /* PTHREADS */

static isc_result_t
ldap_process_results(ldap_instance_t *db, LDAP *dbc, LDAPMessage *msg,
		     char **attrs, void *ptr, isc_boolean_t allnodes)
{
	isc_result_t result = ISC_R_SUCCESS;
	int i = 0;
	int j;
	int len;
	char *attribute = NULL;
	LDAPMessage *entry;
	char *endp = NULL;
	char *host = NULL;
	char *type = NULL;
	char *data = NULL;
	char **vals = NULL;
	int ttl;

	/* get the first entry to process */
	entry = ldap_first_entry(dbc, msg);
	if (entry == NULL) {
		db->log(ISC_LOG_INFO, "LDAP no entries to process.");
		return (ISC_R_FAILURE);
	}

	/* loop through all entries returned */
	while (entry != NULL) {
		/* reset for this loop */
		ttl = 0;
		len = 0;
		i = 0;
		attribute = attrs[i];

		/* determine how much space we need for data string */
		for (j = 0; attrs[j] != NULL; j++) {
			/* get the list of values for this attribute. */
			vals = ldap_get_values(dbc, entry, attrs[j]);
			/* skip empty attributes. */
			if (vals == NULL || ldap_count_values(vals) < 1)
				continue;
			/*
			 * we only use the first value.  this driver
			 * does not support multi-valued attributes.
			 */
			len = len + strlen(vals[0]) + 1;
			/* free vals for next loop */
			ldap_value_free(vals);
		}

		/* allocate memory for data string */
		data = malloc(len + 1);
		if (data == NULL) {
			db->log(ISC_LOG_ERROR,
				"LDAP driver unable to allocate memory "
				"while processing results");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

		/*
		 * Make sure data is null termed at the beginning so
		 * we can check if any data was stored to it later.
		 */
		data[0] = '\0';

		/* reset j to re-use below */
		j = 0;

		/* loop through the attributes in the order specified. */
		while (attribute != NULL) {
			/* get the list of values for this attribute. */
			vals = ldap_get_values(dbc, entry, attribute);

			/* skip empty attributes. */
			if (vals == NULL || vals[0] == NULL) {
				/* increment attibute pointer */
				attribute = attrs[++i];
				/* start loop over */
				continue;
			}

			/*
			 * j initially = 0.  Increment j each time we
			 * set a field that way next loop will set
			 * next field.
			 */
			switch (j) {
			case 0:
				j++;
				/*
				 * convert text to int, make sure it
				 * worked right
				 */
				ttl = strtol(vals[0], &endp, 10);
				if (*endp != '\0' || ttl < 0) {
					db->log(ISC_LOG_ERROR,
						"LDAP driver ttl must "
						"be a postive number");
					goto cleanup;
				}
				break;
			case 1:
				j++;
				type = strdup(vals[0]);
				break;
			case 2:
				j++;
				if (allnodes)
					host = strdup(vals[0]);
				else
					strcpy(data, vals[0]);
				break;
			case 3:
				j++;
				if (allnodes)
					strcpy(data, vals[0]);
				else {
					strcat(data, " ");
					strcat(data, vals[0]);
				}
				break;
			default:
				strcat(data, " ");
				strcat(data, vals[0]);
				break;
			}

			/* free values */
			ldap_value_free(vals);
			vals = NULL;

			/* increment attibute pointer */
			attribute = attrs[++i];
		}

		if (type == NULL) {
			db->log(ISC_LOG_ERROR,
				"LDAP driver unable to retrieve DNS type");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

		if (strlen(data) < 1) {
			db->log(ISC_LOG_ERROR,
				"LDAP driver unable to retrieve DNS data");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

		if (allnodes && host != NULL) {
			dns_sdlzallnodes_t *an = (dns_sdlzallnodes_t *) ptr;
			if (strcasecmp(host, "~") == 0)
				result = db->putnamedrr(an, "*", type,
							ttl, data);
			else
				result = db->putnamedrr(an, host, type,
							ttl, data);
			if (result != ISC_R_SUCCESS)
				db->log(ISC_LOG_ERROR,
					"ldap_dynamic: putnamedrr failed "
					"for \"%s %s %u %s\" (%d)",
					host, type, ttl, data, result);
		} else {
			dns_sdlzlookup_t *lookup = (dns_sdlzlookup_t *) ptr;
			result = db->putrr(lookup, type, ttl, data);
			if (result != ISC_R_SUCCESS)
				db->log(ISC_LOG_ERROR,
					"ldap_dynamic: putrr failed "
					"for \"%s %u %s\" (%s)",
					type, ttl, data, result);
		}

		if (result != ISC_R_SUCCESS) {
			db->log(ISC_LOG_ERROR,
				"LDAP driver failed "
				"while sending data to BIND.");
			goto cleanup;
		}

		/* free memory for type, data and host for next loop */
		free(type);
		type = NULL;

		free(data);
		data = NULL;

		if (host != NULL) {
			free(host);
			host = NULL;
		}

		/* get the next entry to process */
		entry = ldap_next_entry(dbc, entry);
	}

 cleanup:
	/* de-allocate memory */
	if (vals != NULL)
		ldap_value_free(vals);
	if (host != NULL)
		free(host);
	if (type != NULL)
		free(type);
	if (data != NULL)
		free(data);

	return (result);
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
 * obtain a result set.
 */
static isc_result_t
ldap_get_results(const char *zone, const char *record,
		 const char *client, unsigned int query,
		 void *dbdata, void *ptr)
{
	isc_result_t result;
	ldap_instance_t *db = (ldap_instance_t *)dbdata;
	dbinstance_t *dbi = NULL;
	char *querystring = NULL;
	LDAPURLDesc *ldap_url = NULL;
	int ldap_result = 0;
	LDAPMessage *ldap_msg = NULL;
	int i;
	int entries;

	/* get db instance / connection */
#if PTHREADS
	/* find an available DBI from the list */
	dbi = ldap_find_avail_conn(db);
#else /* PTHREADS */
	/*
	 * only 1 DBI - no need to lock instance lock either
	 * only 1 thread in the whole process, no possible contention.
	 */
	dbi = (dbinstance_t *)(db->db);
#endif /* PTHREADS */

	/* if DBI is null, can't do anything else */
	if (dbi == NULL)
		return (ISC_R_FAILURE);

	/* set fields */
	if (zone != NULL) {
		dbi->zone = strdup(zone);
		if (dbi->zone == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else
		dbi->zone = NULL;

	if (record != NULL) {
		dbi->record = strdup(record);
		if (dbi->record == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else
		dbi->record = NULL;

	if (client != NULL) {
		dbi->client = strdup(client);
		if (dbi->client == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else
		dbi->client = NULL;


	/* what type of query are we going to run? */
	switch (query) {
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
		} else
			querystring = build_querystring(dbi->allnodes_q);
		break;
	case ALLOWXFR:
		/* same as comments as ALLNODES */
		if (dbi->allowxfr_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		} else
			querystring = build_querystring(dbi->allowxfr_q);
		break;
	case AUTHORITY:
		/* same as comments as ALLNODES */
		if (dbi->authority_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		} else
			querystring = build_querystring(dbi->authority_q);
		break;
	case FINDZONE:
		/* this is required.  It's the whole point of DLZ! */
		if (dbi->findzone_q == NULL) {
			db->log(ISC_LOG_DEBUG(2),
				"No query specified for findzone. "
				"Findzone requires a query");
			result = ISC_R_FAILURE;
			goto cleanup;
		} else
			querystring = build_querystring(dbi->findzone_q);
		break;
	case LOOKUP:
		/* this is required.  It's also a major point of DLZ! */
		if (dbi->lookup_q == NULL) {
			db->log(ISC_LOG_DEBUG(2),
				"No query specified for lookup. "
				"Lookup requires a query");
			result = ISC_R_FAILURE;
			goto cleanup;
		} else
			querystring = build_querystring(dbi->lookup_q);
		break;
	default:
		/*
		 * this should never happen.  If it does, the code is
		 * screwed up!
		 */
		db->log(ISC_LOG_ERROR,
			"Incorrect query flag passed to ldap_get_results");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

	/* if the querystring is null, Bummer, outta RAM.  UPGRADE TIME!!!   */
	if (querystring  == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/*
	 * output the full query string during debug so we can see
	 * what lame error the query has.
	 */
	db->log(ISC_LOG_DEBUG(1), "Query String: %s", querystring);

	/* break URL down into it's component parts, if error cleanup */
	ldap_result = ldap_url_parse(querystring, &ldap_url);
	if (ldap_result != LDAP_SUCCESS || ldap_url == NULL) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	for (i = 0; i < 3; i++) {
		/*
		 * dbi->dbconn may be null if trying to reconnect on a
		 * previous query failed.
		 */
		if (dbi->dbconn == NULL) {
			db->log(ISC_LOG_INFO,
				"LDAP driver attempting to re-connect");

			result = ldap_connect((ldap_instance_t *) dbdata, dbi);
			if (result != ISC_R_SUCCESS) {
				result = ISC_R_FAILURE;
				continue;
			}
		}

		/* perform ldap search syncronously */
		ldap_result = ldap_search_s((LDAP *) dbi->dbconn,
					    ldap_url->lud_dn,
					    ldap_url->lud_scope,
					    ldap_url->lud_filter,
					    ldap_url->lud_attrs, 0, &ldap_msg);

		/*
		 * check return code.  No such object is ok, just
		 * didn't find what we wanted
		 */
		switch (ldap_result) {
		case LDAP_NO_SUCH_OBJECT:
    			db->log(ISC_LOG_DEBUG(1),
				"No object found matching query requirements");
			result = ISC_R_NOTFOUND;
			goto cleanup;
			break;
		case LDAP_SUCCESS:	/* on success do nothing */
			result = ISC_R_SUCCESS;
			i = 3;
			break;
		case LDAP_SERVER_DOWN:
			db->log(ISC_LOG_INFO,
				"LDAP driver attempting to re-connect");
			result = ldap_connect((ldap_instance_t *) dbdata, dbi);
			if (result != ISC_R_SUCCESS)
				result = ISC_R_FAILURE;
			break;
		default:
			/*
			 * other errors not ok.  Log error message and
			 * get out
			 */
    			db->log(ISC_LOG_ERROR, "LDAP error: %s",
				ldap_err2string(ldap_result));
			result = ISC_R_FAILURE;
			goto cleanup;
			break;
		}
	}

	if (result != ISC_R_SUCCESS)
		goto cleanup;

	switch (query) {
	case ALLNODES:
		result = ldap_process_results(db, (LDAP *) dbi->dbconn,
					      ldap_msg, ldap_url->lud_attrs,
					      ptr, ISC_TRUE);
		break;
	case AUTHORITY:
	case LOOKUP:
		result = ldap_process_results(db, (LDAP *) dbi->dbconn,
					      ldap_msg, ldap_url->lud_attrs,
					      ptr, ISC_FALSE);
		break;
	case ALLOWXFR:
		entries = ldap_count_entries((LDAP *) dbi->dbconn, ldap_msg);
		if (entries == 0)
			result = ISC_R_NOPERM;
		else if (entries > 0)
			result = ISC_R_SUCCESS;
		else
			result = ISC_R_FAILURE;
		break;
	case FINDZONE:
		entries = ldap_count_entries((LDAP *) dbi->dbconn, ldap_msg);
		if (entries == 0)
			result = ISC_R_NOTFOUND;
		else if (entries > 0)
			result = ISC_R_SUCCESS;
		else
			result = ISC_R_FAILURE;
		break;
	default:
		/*
		 * this should never happen.  If it does, the code is
		 * screwed up!
		 */
		db->log(ISC_LOG_ERROR,
			"Incorrect query flag passed to ldap_get_results");
		result = ISC_R_UNEXPECTED;
	}

 cleanup:
	/* it's always good to cleanup after yourself */

	/* if we retrieved results, free them */
	if (ldap_msg != NULL)
		ldap_msgfree(ldap_msg);

	if (ldap_url != NULL)
		ldap_free_urldesc(ldap_url);

	/* cleanup */
	if (dbi->zone != NULL)
		free(dbi->zone);
	if (dbi->record != NULL)
		free(dbi->record);
	if (dbi->client != NULL)
		free(dbi->client);
	dbi->zone = dbi->record = dbi->client = NULL;

	/* release the lock so another thread can use this dbi */
	(void) dlz_mutex_unlock(&dbi->lock);

	/* release query string */
	if (querystring != NULL)
		free(querystring);

	/* return result */
	return (result);
}

/*
 * DLZ methods
 */
isc_result_t
dlz_allowzonexfr(void *dbdata, const char *name, const char *client) {
	isc_result_t result;

	/* check to see if we are authoritative for the zone first */
#if DLZ_DLOPEN_VERSION < 3
	result = dlz_findzonedb(dbdata, name);
#else
	result = dlz_findzonedb(dbdata, name, NULL, NULL);
#endif
	if (result != ISC_R_SUCCESS) {
		return (result);
	}

	/* get all the zone data */
	result = ldap_get_results(name, NULL, client, ALLOWXFR, dbdata, NULL);
	return (result);
}

isc_result_t
dlz_allnodes(const char *zone, void *dbdata, dns_sdlzallnodes_t *allnodes)
{
	return (ldap_get_results(zone, NULL, NULL, ALLNODES, dbdata, allnodes));
}

isc_result_t
dlz_authority(const char *zone, void *dbdata, dns_sdlzlookup_t *lookup) {
	return (ldap_get_results(zone, NULL, NULL, AUTHORITY, dbdata, lookup));
}

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
#if DLZ_DLOPEN_VERSION >= 3
	UNUSED(methods);
	UNUSED(clientinfo);
#endif
	return (ldap_get_results(name, NULL, NULL, FINDZONE, dbdata, NULL));
}

#if DLZ_DLOPEN_VERSION == 1
isc_result_t dlz_lookup(const char *zone, const char *name, 
			void *dbdata, dns_sdlzlookup_t *lookup)
#else
isc_result_t dlz_lookup(const char *zone, const char *name,
			void *dbdata, dns_sdlzlookup_t *lookup,
			dns_clientinfomethods_t *methods,
			dns_clientinfo_t *clientinfo)
#endif
{
	isc_result_t result;

#if DLZ_DLOPEN_VERSION >= 2
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	if (strcmp(name, "*") == 0)
		result = ldap_get_results(zone, "~", NULL, LOOKUP,
					  dbdata, lookup);
	else
		result = ldap_get_results(zone, name, NULL, LOOKUP,
					  dbdata, lookup);
	return (result);
}


isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[],
	   void **dbdata, ...)
{
	isc_result_t result = ISC_R_FAILURE;
	ldap_instance_t *ldap = NULL;
	dbinstance_t *dbi = NULL;
	const char *helper_name;
	int protocol;
	int method;
#if PTHREADS
	int dbcount;
	char *endp;
	int i;
#endif /* PTHREADS */
	va_list ap;

	UNUSED(dlzname);

	/* allocate memory for LDAP instance */
	ldap = calloc(1, sizeof(ldap_instance_t));
	if (ldap == NULL)
		return (ISC_R_NOMEMORY);
	memset(ldap, 0, sizeof(ldap_instance_t));

	/* Fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char*)) != NULL)
		b9_add_helper(ldap, helper_name, va_arg(ap, void*));
	va_end(ap);

#if PTHREADS
	/* if debugging, let user know we are multithreaded. */
	ldap->log(ISC_LOG_DEBUG(1), "LDAP driver running multithreaded");
#else /* PTHREADS */
	/* if debugging, let user know we are single threaded. */
	ldap->log(ISC_LOG_DEBUG(1), "LDAP driver running single threaded");
#endif /* PTHREADS */

	if (argc < 9) {
		ldap->log(ISC_LOG_ERROR,
			  "LDAP driver requires at least "
			  "8 command line args.");
		goto cleanup;
	}

	/* no more than 13 arg's should be passed to the driver */
	if (argc > 12) {
		ldap->log(ISC_LOG_ERROR,
			  "LDAP driver cannot accept more than "
			  "11 command line args.");
		goto cleanup;
	}

	/* determine protocol version. */
	if (strncasecmp(argv[2], V2, strlen(V2)) == 0)
		protocol = 2;
	else if (strncasecmp(argv[2], V3, strlen(V3)) == 0)
		protocol = 3;
	else {
		ldap->log(ISC_LOG_ERROR,
			  "LDAP driver protocol must be either %s or %s",
			  V2, V3);
		goto cleanup;
	}

	/* determine connection method. */
	if (strncasecmp(argv[3], SIMPLE, strlen(SIMPLE)) == 0)
		method = LDAP_AUTH_SIMPLE;
	else if (strncasecmp(argv[3], KRB41, strlen(KRB41)) == 0)
		method = LDAP_AUTH_KRBV41;
	else if (strncasecmp(argv[3], KRB42, strlen(KRB42)) == 0)
		method = LDAP_AUTH_KRBV42;
	else {
		ldap->log(ISC_LOG_ERROR,
			  "LDAP driver authentication method must be "
			  "one of %s, %s or %s", SIMPLE, KRB41, KRB42);
		goto cleanup;
	}

	/* multithreaded build can have multiple DB connections */
#if PTHREADS
	/* check how many db connections we should create */
	dbcount = strtol(argv[1], &endp, 10);
	if (*endp != '\0' || dbcount < 0) {
		ldap->log(ISC_LOG_ERROR,
			  "LDAP driver database connection count "
			  "must be positive.");
		goto cleanup;
	}
#endif

	/* check that LDAP URL parameters make sense */
	switch (argc) {
	case 12:
		result = ldap_checkURL(ldap, argv[11], 0,
				       "allow zone transfer");
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	case 11:
		result = ldap_checkURL(ldap, argv[10], 3, "all nodes");
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	case 10:
		if (strlen(argv[9]) > 0) {
			result = ldap_checkURL(ldap, argv[9], 3, "authority");
			if (result != ISC_R_SUCCESS)
				goto cleanup;
		}
	case 9:
		result = ldap_checkURL(ldap, argv[8], 3, "lookup");
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = ldap_checkURL(ldap, argv[7], 0, "find zone");
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		break;
	default:
		/* not really needed, should shut up compiler. */
		result = ISC_R_FAILURE;
	}

	/* store info needed to automatically re-connect. */
	ldap->protocol = protocol;
	ldap->method = method;
	ldap->hosts = strdup(argv[6]);
	if (ldap->hosts == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	ldap->user = strdup(argv[4]);
	if (ldap->user == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	ldap->cred = strdup(argv[5]);
	if (ldap->cred == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

#if PTHREADS
	/* allocate memory for database connection list */
	ldap->db = calloc(1, sizeof(db_list_t));
	if (ldap->db == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* initialize DB connection list */
	DLZ_LIST_INIT(*(ldap->db));

	/*
	 * create the appropriate number of database instances (DBI)
	 * append each new DBI to the end of the list
	 */
	for (i = 0; i < dbcount; i++) {
#endif /* PTHREADS */
		/* how many queries were passed in from config file? */
		switch (argc) {
		case 9:
			result = build_dbinstance(NULL, NULL, NULL, argv[7],
						  argv[8], NULL, &dbi,
						  ldap->log);
			break;
		case 10:
			result = build_dbinstance(NULL, NULL, argv[9],
						  argv[7], argv[8],
						  NULL, &dbi, ldap->log);
			break;
		case 11:
			result = build_dbinstance(argv[10], NULL, argv[9],
						  argv[7], argv[8],
						  NULL, &dbi, ldap->log);
			break;
		case 12:
			result = build_dbinstance(argv[10], argv[11],
						  argv[9], argv[7],
						  argv[8], NULL, &dbi,
						  ldap->log);
			break;
		default:
			/* not really needed, should shut up compiler. */
			result = ISC_R_FAILURE;
		}

		if (result == ISC_R_SUCCESS) {
			ldap->log(ISC_LOG_DEBUG(2),
				  "LDAP driver created "
				  "database instance object.");
		} else { /* unsuccessful?, log err msg and cleanup. */
			ldap->log(ISC_LOG_ERROR,
				  "LDAP driver could not create "
				  "database instance object.");
			goto cleanup;
		}

#if PTHREADS
		/* when multithreaded, build a list of DBI's */
		DLZ_LINK_INIT(dbi, link);
		DLZ_LIST_APPEND(*(ldap->db), dbi, link);
#else
		/*
		 * when single threaded, hold onto the one connection
		 * instance.
		 */
		ldap->db = dbi;
#endif
		/* attempt to connect */
		result = ldap_connect(ldap, dbi);

		/*
		 * if db connection cannot be created, log err msg and
		 * cleanup.
		 */
		switch (result) {
			/* success, do nothing */
		case ISC_R_SUCCESS:
			break;
			/*
			 * no memory means ldap_init could not
			 * allocate memory
			 */
		case ISC_R_NOMEMORY:
#if PTHREADS
			ldap->log(ISC_LOG_ERROR,
				  "LDAP driver could not allocate memory "
				  "for connection number %u", i + 1);
#else
			ldap->log(ISC_LOG_ERROR,
				  "LDAP driver could not allocate memory "
				  "for connection");
#endif
			goto cleanup;
			/*
			 * no perm means ldap_set_option could not set
			 * protocol version
			 */
		case ISC_R_NOPERM:
			ldap->log(ISC_LOG_ERROR,
				  "LDAP driver could not "
				  "set protocol version.");
			result = ISC_R_FAILURE;
			goto cleanup;
			/* failure means couldn't connect to ldap server */
		case ISC_R_FAILURE:
#if PTHREADS
			ldap->log(ISC_LOG_ERROR,
				  "LDAP driver could not bind "
				  "connection number %u to server.", i + 1);
#else
			ldap->log(ISC_LOG_ERROR,
				  "LDAP driver could not "
				  "bind connection to server.");
#endif
			goto cleanup;
			/*
			 * default should never happen.  If it does,
			 * major errors.
			 */
		default:
			ldap->log(ISC_LOG_ERROR,
				  "dlz_create() failed (%d)", result);
			result = ISC_R_UNEXPECTED;
			goto cleanup;
		}

#if PTHREADS
		/* set DBI = null for next loop through. */
		dbi = NULL;
	}
#endif /* PTHREADS */

	/* set dbdata to the ldap_instance we created. */
	*dbdata = ldap;

	return (ISC_R_SUCCESS);

 cleanup:
	dlz_destroy(ldap);

	return (result);
}

void
dlz_destroy(void *dbdata) {
	if (dbdata != NULL) {
		ldap_instance_t *db = (ldap_instance_t *)dbdata;
#if PTHREADS
		/* cleanup the list of DBI's */
		if (db->db != NULL)
			ldap_destroy_dblist((db_list_t *)(db->db));
#else /* PTHREADS */
		if (db->db->dbconn != NULL)
			ldap_unbind_s((LDAP *)(db->db->dbconn));

		/* destroy single DB instance */
		destroy_dbinstance(db->db);
#endif /* PTHREADS */

		if (db->hosts != NULL)
			free(db->hosts);
		if (db->user != NULL)
			free(db->user);
		if (db->cred != NULL)
			free(db->cred);
		free(dbdata);
	}
}

/*
 * Return the version of the API
 */
int
dlz_version(unsigned int *flags) {
	*flags |= DNS_SDLZFLAG_RELATIVERDATA;
#if PTHREADS
	*flags |= DNS_SDLZFLAG_THREADSAFE;
#else
	*flags &= ~DNS_SDLZFLAG_THREADSAFE;
#endif
	return (DLZ_DLOPEN_VERSION);
}

/*
 * Register a helper function from the bind9 dlz_dlopen driver
 */
static void
b9_add_helper(ldap_instance_t *db, const char *helper_name, void *ptr) {
	if (strcmp(helper_name, "log") == 0)
		db->log = (log_t *)ptr;
	if (strcmp(helper_name, "putrr") == 0)
		db->putrr = (dns_sdlz_putrr_t *)ptr;
	if (strcmp(helper_name, "putnamedrr") == 0)
		db->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
	if (strcmp(helper_name, "writeable_zone") == 0)
		db->writeable_zone = (dns_dlz_writeablezone_t *)ptr;
}
