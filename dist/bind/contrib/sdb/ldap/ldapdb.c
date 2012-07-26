/*
 * ldapdb.c version 1.0-beta
 *
 * Copyright (C) 2002, 2004 Stig Venaas
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * Contributors: Jeremy C. McDermond
 */

/*
 * If you want to use TLS, uncomment the define below
 */
/* #define LDAPDB_TLS */

/*
 * If you are using an old LDAP API uncomment the define below. Only do this
 * if you know what you're doing or get compilation errors on ldap_memfree().
 * This also forces LDAPv2.
 */
/* #define LDAPDB_RFC1823API */

/* Using LDAPv3 by default, change this if you want v2 */
#ifndef LDAPDB_LDAP_VERSION
#define LDAPDB_LDAP_VERSION 3
#endif

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>
#include <isc/thread.h>

#include <dns/sdb.h>

#include <named/globals.h>
#include <named/log.h>

#include <ldap.h>
#include "ldapdb.h"

/*
 * A simple database driver for LDAP
 */ 

/* enough for name with 8 labels of max length */
#define MAXNAMELEN 519

static dns_sdbimplementation_t *ldapdb = NULL;

struct ldapdb_data {
	char *hostport;
	char *hostname;
	int portno;
	char *base;
	int defaultttl;
	char *filterall;
	int filteralllen;
	char *filterone;
	int filteronelen;
	char *filtername;
	char *bindname;
	char *bindpw;
#ifdef LDAPDB_TLS
	int tls;
#endif
};

/* used by ldapdb_getconn */

struct ldapdb_entry {
	void *index;
	size_t size;
	void *data;
	struct ldapdb_entry *next;
};

static struct ldapdb_entry *ldapdb_find(struct ldapdb_entry *stack,
					const void *index, size_t size) {
	while (stack != NULL) {
		if (stack->size == size && !memcmp(stack->index, index, size))
			return stack;
		stack = stack->next;
	}
	return NULL;
}

static void ldapdb_insert(struct ldapdb_entry **stack,
			  struct ldapdb_entry *item) {
	item->next = *stack;
	*stack = item;
}

static void ldapdb_lock(int what) {
	static isc_mutex_t lock;

	switch (what) {
	case 0:
		isc_mutex_init(&lock);
		break;
	case 1:
		LOCK(&lock);
		break;
	case -1:
		UNLOCK(&lock);
		break;
	}
}

/* data == NULL means cleanup */
static LDAP **
ldapdb_getconn(struct ldapdb_data *data)
{
	static struct ldapdb_entry *allthreadsdata = NULL;
	struct ldapdb_entry *threaddata, *conndata;
	unsigned long threadid;

	if (data == NULL) {
		/* cleanup */
		/* lock out other threads */
		ldapdb_lock(1);
		while (allthreadsdata != NULL) {
			threaddata = allthreadsdata;
			free(threaddata->index);
			while (threaddata->data != NULL) {
				conndata = threaddata->data;
				free(conndata->index);
				if (conndata->data != NULL)
					ldap_unbind((LDAP *)conndata->data);
				threaddata->data = conndata->next;
				free(conndata);
			}
			allthreadsdata = threaddata->next;
			free(threaddata);
		}
		ldapdb_lock(-1);
		return (NULL);
	}

	/* look for connection data for current thread */
	threadid = isc_thread_self();
	threaddata = ldapdb_find(allthreadsdata, &threadid, sizeof(threadid));
	if (threaddata == NULL) {
		/* no data for this thread, create empty connection list */
		threaddata = malloc(sizeof(*threaddata));
		if (threaddata == NULL)
			return (NULL);
		threaddata->index = malloc(sizeof(threadid));
		if (threaddata->index == NULL) {
			free(threaddata);
			return (NULL);
		}
		*(unsigned long *)threaddata->index = threadid;
		threaddata->size = sizeof(threadid);
		threaddata->data = NULL;

		/* need to lock out other threads here */
		ldapdb_lock(1);
		ldapdb_insert(&allthreadsdata, threaddata);
		ldapdb_lock(-1);
	}

	/* threaddata points at the connection list for current thread */
	/* look for existing connection to our server */
	conndata = ldapdb_find((struct ldapdb_entry *)threaddata->data,
			       data->hostport, strlen(data->hostport));
	if (conndata == NULL) {
		/* no connection data structure for this server, create one */
		conndata = malloc(sizeof(*conndata));
		if (conndata == NULL)
			return (NULL);
		conndata->index = data->hostport;
		conndata->size = strlen(data->hostport);
		conndata->data = NULL;
		ldapdb_insert((struct ldapdb_entry **)&threaddata->data,
			      conndata);
	}

	return (LDAP **)&conndata->data;
}

static void
ldapdb_bind(struct ldapdb_data *data, LDAP **ldp)
{
#ifndef LDAPDB_RFC1823API
	const int ver = LDAPDB_LDAP_VERSION;
#endif

	if (*ldp != NULL)
		ldap_unbind(*ldp);
	*ldp = ldap_open(data->hostname, data->portno);
	if (*ldp == NULL)
		return;

#ifndef LDAPDB_RFC1823API
	ldap_set_option(*ldp, LDAP_OPT_PROTOCOL_VERSION, &ver);
#endif

#ifdef LDAPDB_TLS
	if (data->tls) {
		ldap_start_tls_s(*ldp, NULL, NULL);
	}
#endif

	if (ldap_simple_bind_s(*ldp, data->bindname, data->bindpw) != LDAP_SUCCESS) {
		ldap_unbind(*ldp);
		*ldp = NULL;
	}
}

static isc_result_t
ldapdb_search(const char *zone, const char *name, void *dbdata, void *retdata)
{
	struct ldapdb_data *data = dbdata;
	isc_result_t result = ISC_R_NOTFOUND;
	LDAP **ldp;
	LDAPMessage *res, *e;
	char *fltr, *a, **vals = NULL, **names = NULL;
	char type[64];
#ifdef LDAPDB_RFC1823API
	void *ptr;
#else
	BerElement *ptr;
#endif
	int i, j, errno, msgid;

	ldp = ldapdb_getconn(data);
	if (ldp == NULL)
		return (ISC_R_FAILURE);
	if (*ldp == NULL) {
		ldapdb_bind(data, ldp);
		if (*ldp == NULL) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,	
				      "LDAP sdb zone '%s': bind failed", zone);
			return (ISC_R_FAILURE);
		}
	}

	if (name == NULL) {
		fltr = data->filterall;
	} else {
		if (strlen(name) > MAXNAMELEN) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
                                      "LDAP sdb zone '%s': name %s too long", zone, name);
			return (ISC_R_FAILURE);
		}
		sprintf(data->filtername, "%s))", name);
		fltr = data->filterone;
	}

	msgid = ldap_search(*ldp, data->base, LDAP_SCOPE_SUBTREE, fltr, NULL, 0);
	if (msgid == -1) {
		ldapdb_bind(data, ldp);
		if (*ldp != NULL)
			msgid = ldap_search(*ldp, data->base, LDAP_SCOPE_SUBTREE, fltr, NULL, 0);
	}

	if (*ldp == NULL || msgid == -1) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,	
			      "LDAP sdb zone '%s': search failed, filter %s", zone, fltr);
		return (ISC_R_FAILURE);
	}

	/* Get the records one by one as they arrive and return them to bind */
	while ((errno = ldap_result(*ldp, msgid, 0, NULL, &res)) != LDAP_RES_SEARCH_RESULT ) {
		LDAP *ld = *ldp;
		int ttl = data->defaultttl;

		/* not supporting continuation references at present */
		if (errno != LDAP_RES_SEARCH_ENTRY) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,	
				      "LDAP sdb zone '%s': ldap_result returned %d", zone, errno);
			ldap_msgfree(res);
			return (ISC_R_FAILURE);
                }

		/* only one entry per result message */
		e = ldap_first_entry(ld, res);
		if (e == NULL) {
			ldap_msgfree(res);
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,	
				      "LDAP sdb zone '%s': ldap_first_entry failed", zone);
			return (ISC_R_FAILURE);
                }

		if (name == NULL) {
			names = ldap_get_values(ld, e, "relativeDomainName");
			if (names == NULL)
				continue;
		}

		vals = ldap_get_values(ld, e, "dNSTTL");
		if (vals != NULL) {
			ttl = atoi(vals[0]);
			ldap_value_free(vals);
		}

		for (a = ldap_first_attribute(ld, e, &ptr); a != NULL; a = ldap_next_attribute(ld, e, ptr)) {
			char *s;

			for (s = a; *s; s++)
				*s = toupper(*s);
			s = strstr(a, "RECORD");
			if ((s == NULL) || (s == a) || (s - a >= (signed int)sizeof(type))) {
#ifndef LDAPDB_RFC1823API
				ldap_memfree(a);
#endif
				continue;
			}

			strncpy(type, a, s - a);
			type[s - a] = '\0';
			vals = ldap_get_values(ld, e, a);
			if (vals != NULL) {
				for (i = 0; vals[i] != NULL; i++) {
					if (name != NULL) {
						result = dns_sdb_putrr(retdata, type, ttl, vals[i]);
					} else {
						for (j = 0; names[j] != NULL; j++) {
							result = dns_sdb_putnamedrr(retdata, names[j], type, ttl, vals[i]);
							if (result != ISC_R_SUCCESS)
								break;
						}
					}
;					if (result != ISC_R_SUCCESS) {
						isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,	
							      "LDAP sdb zone '%s': dns_sdb_put... failed for %s", zone, vals[i]);
						ldap_value_free(vals);
#ifndef LDAPDB_RFC1823API
						ldap_memfree(a);
						if (ptr != NULL)
							ber_free(ptr, 0);
#endif
						if (name == NULL)
							ldap_value_free(names);
						ldap_msgfree(res);
						return (ISC_R_FAILURE);
					}
				}
				ldap_value_free(vals);
			}
#ifndef LDAPDB_RFC1823API
			ldap_memfree(a);
#endif
		}
#ifndef LDAPDB_RFC1823API
		if (ptr != NULL)
			ber_free(ptr, 0);
#endif
		if (name == NULL)
			ldap_value_free(names);

		/* free this result */
		ldap_msgfree(res);
	}

	/* free final result */
	ldap_msgfree(res);
        return (result);
}


/* callback routines */
static isc_result_t
ldapdb_lookup(const char *zone, const char *name, void *dbdata,
	      dns_sdblookup_t *lookup)
{
	return ldapdb_search(zone, name, dbdata, lookup);
}

static isc_result_t
ldapdb_allnodes(const char *zone, void *dbdata,
		dns_sdballnodes_t *allnodes)
{
	return ldapdb_search(zone, NULL, dbdata, allnodes);
}

static char *
unhex(char *in)
{
	static const char hexdigits[] = "0123456789abcdef";
	char *p, *s = in;
	int d1, d2;

	while ((s = strchr(s, '%'))) {
		if (!(s[1] && s[2]))
			return NULL;
		if ((p = strchr(hexdigits, tolower(s[1]))) == NULL)
			return NULL;
		d1 = p - hexdigits;
		if ((p = strchr(hexdigits, tolower(s[2]))) == NULL)
			return NULL;
		d2 = p - hexdigits;
		*s++ = d1 << 4 | d2;
		memmove(s, s + 2, strlen(s) - 1);
	}
	return in;
}

/* returns 0 for ok, -1 for bad syntax, -2 for unknown critical extension */
static int
parseextensions(char *extensions, struct ldapdb_data *data)
{
	char *s, *next, *name, *value;
	int critical;

	while (extensions != NULL) {
		s = strchr(extensions, ',');
		if (s != NULL) {
			*s++ = '\0';
			next = s;
		} else {
			next = NULL;
		}

		if (*extensions != '\0') {
			s = strchr(extensions, '=');
			if (s != NULL) {
				*s++ = '\0';
				value = *s != '\0' ? s : NULL;
			} else {
				value = NULL;
			}
			name = extensions;

			critical = *name == '!';
			if (critical) {
				name++;
			}
			if (*name == '\0') {
				return -1;
			}
			
			if (!strcasecmp(name, "bindname")) {
				data->bindname = value;
			} else if (!strcasecmp(name, "x-bindpw")) {
				data->bindpw = value;
#ifdef LDAPDB_TLS
			} else if (!strcasecmp(name, "x-tls")) {
				data->tls = value == NULL || !strcasecmp(value, "true");
#endif
			} else if (critical) {
				return -2;
			}
		}
		extensions = next;
	}
	return 0;
}

static void
free_data(struct ldapdb_data *data)
{
	if (data->hostport != NULL)
		isc_mem_free(ns_g_mctx, data->hostport);
	if (data->hostname != NULL)
		isc_mem_free(ns_g_mctx, data->hostname);
	if (data->filterall != NULL)
		isc_mem_put(ns_g_mctx, data->filterall, data->filteralllen);
	if (data->filterone != NULL)
		isc_mem_put(ns_g_mctx, data->filterone, data->filteronelen);
        isc_mem_put(ns_g_mctx, data, sizeof(struct ldapdb_data));
}


static isc_result_t
ldapdb_create(const char *zone, int argc, char **argv,
	      void *driverdata, void **dbdata)
{
	struct ldapdb_data *data;
	char *s, *filter = NULL, *extensions = NULL;
	int defaultttl;

	UNUSED(driverdata);

	/* we assume that only one thread will call create at a time */
	/* want to do this only once for all instances */

	if ((argc < 2)
	    || (argv[0] != strstr( argv[0], "ldap://"))
	    || ((defaultttl = atoi(argv[1])) < 1))
                return (ISC_R_FAILURE);
        data = isc_mem_get(ns_g_mctx, sizeof(struct ldapdb_data));
        if (data == NULL)
                return (ISC_R_NOMEMORY);

	memset(data, 0, sizeof(struct ldapdb_data));
	data->hostport = isc_mem_strdup(ns_g_mctx, argv[0] + strlen("ldap://"));
	if (data->hostport == NULL) {
		free_data(data);
		return (ISC_R_NOMEMORY);
	}

	data->defaultttl = defaultttl;

	s = strchr(data->hostport, '/');
	if (s != NULL) {
		*s++ = '\0';
		data->base = s;
		/* attrs, scope, filter etc? */
		s = strchr(s, '?');
		if (s != NULL) {
			*s++ = '\0';
			/* ignore attributes */
			s = strchr(s, '?');
			if (s != NULL) {
				*s++ = '\0';
				/* ignore scope */
				s = strchr(s, '?');
				if (s != NULL) {
					*s++ = '\0';
					/* filter */
					filter = s;
					s = strchr(s, '?');
					if (s != NULL) {
						*s++ = '\0';
						/* extensions */
						extensions = s;
						s = strchr(s, '?');
						if (s != NULL) {
							*s++ = '\0';
						}
						if (*extensions == '\0') {
							extensions = NULL;
						}
					}
					if (*filter == '\0') {
						filter = NULL;
					}
				}
			}
		}
		if (*data->base == '\0') {
			data->base = NULL;
		}
	}

	/* parse extensions */
	if (extensions != NULL) {
		int err;

		err = parseextensions(extensions, data);
		if (err < 0) {
			/* err should be -1 or -2 */
			free_data(data);
			if (err == -1) {
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
					      "LDAP sdb zone '%s': URL: extension syntax error", zone);
			} else if (err == -2) {
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
					      "LDAP sdb zone '%s': URL: unknown critical extension", zone);
			}
			return (ISC_R_FAILURE);
		}
	}

	if ((data->base != NULL && unhex(data->base) == NULL) ||
	    (filter != NULL && unhex(filter) == NULL) ||
	    (data->bindname != NULL && unhex(data->bindname) == NULL) ||
	    (data->bindpw != NULL && unhex(data->bindpw) == NULL)) {
		free_data(data);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER, ISC_LOG_ERROR,	
			      "LDAP sdb zone '%s': URL: bad hex values", zone);
		return (ISC_R_FAILURE);
	}

	/* compute filterall and filterone once and for all */
	if (filter == NULL) {
		data->filteralllen = strlen(zone) + strlen("(zoneName=)") + 1;
		data->filteronelen = strlen(zone) + strlen("(&(zoneName=)(relativeDomainName=))") + MAXNAMELEN + 1;
	} else {
		data->filteralllen = strlen(filter) + strlen(zone) + strlen("(&(zoneName=))") + 1;
		data->filteronelen = strlen(filter) + strlen(zone) + strlen("(&(zoneName=)(relativeDomainName=))") + MAXNAMELEN + 1;
	}

	data->filterall = isc_mem_get(ns_g_mctx, data->filteralllen);
	if (data->filterall == NULL) {
		free_data(data);
		return (ISC_R_NOMEMORY);
	}
	data->filterone = isc_mem_get(ns_g_mctx, data->filteronelen);
	if (data->filterone == NULL) {
		free_data(data);
		return (ISC_R_NOMEMORY);
	}

	if (filter == NULL) {
		sprintf(data->filterall, "(zoneName=%s)", zone);
		sprintf(data->filterone, "(&(zoneName=%s)(relativeDomainName=", zone); 
	} else {
		sprintf(data->filterall, "(&%s(zoneName=%s))", filter, zone);
		sprintf(data->filterone, "(&%s(zoneName=%s)(relativeDomainName=", filter, zone);
	}
	data->filtername = data->filterone + strlen(data->filterone);

	/* support URLs with literal IPv6 addresses */
	data->hostname = isc_mem_strdup(ns_g_mctx, data->hostport + (*data->hostport == '[' ? 1 : 0));
	if (data->hostname == NULL) {
		free_data(data);
		return (ISC_R_NOMEMORY);
	}

	if (*data->hostport == '[' &&
	    (s = strchr(data->hostname, ']')) != NULL )
		*s++ = '\0';
	else
		s = data->hostname;
	s = strchr(s, ':');
	if (s != NULL) {
		*s++ = '\0';
		data->portno = atoi(s);
	} else
		data->portno = LDAP_PORT;

	*dbdata = data;
	return (ISC_R_SUCCESS);
}

static void
ldapdb_destroy(const char *zone, void *driverdata, void **dbdata) {
	struct ldapdb_data *data = *dbdata;
	
        UNUSED(zone);
        UNUSED(driverdata);

	free_data(data);
}

static dns_sdbmethods_t ldapdb_methods = {
	ldapdb_lookup,
	NULL, /* authority */
	ldapdb_allnodes,
	ldapdb_create,
	ldapdb_destroy
};

/* Wrapper around dns_sdb_register() */
isc_result_t
ldapdb_init(void) {
	unsigned int flags =
		DNS_SDBFLAG_RELATIVEOWNER |
		DNS_SDBFLAG_RELATIVERDATA |
		DNS_SDBFLAG_THREADSAFE;

	ldapdb_lock(0);
	return (dns_sdb_register("ldap", &ldapdb_methods, NULL, flags,
				 ns_g_mctx, &ldapdb));
}

/* Wrapper around dns_sdb_unregister() */
void
ldapdb_clear(void) {
	if (ldapdb != NULL) {
		/* clean up thread data */
		ldapdb_getconn(NULL);
		dns_sdb_unregister(&ldapdb);
	}
}
