/*	$NetBSD: dlz_wildcard_dynamic.c,v 1.3 2014/12/10 04:37:55 christos Exp $	*/

/*
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 * Copyright (C) 2012 Vadim Goncharov, Russia, vadim_nuclight@mail.ru.
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

/*
 * This provides the externally loadable wildcard DLZ module.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <dlz_minimal.h>
#include <dlz_list.h>
#include <dlz_dbi.h>

#include <ctype.h>

#define DE_CONST(konst, var) \
	do { \
		union { const void *k; void *v; } _u; \
		_u.k = konst; \
		var = _u.v; \
	} while (/*CONSTCOND*/0)

/* fnmatch() return values. */
#define	FNM_NOMATCH	1	/* Match failed. */

/* fnmatch() flags. */
#define	FNM_NOESCAPE	0x01	/* Disable backslash escaping. */
#define	FNM_PATHNAME	0x02	/* Slash must be matched by slash. */
#define	FNM_PERIOD	0x04	/* Period must be matched by period. */
#define	FNM_LEADING_DIR	0x08	/* Ignore /<tail> after Imatch. */
#define	FNM_CASEFOLD	0x10	/* Case insensitive search. */
#define	FNM_IGNORECASE	FNM_CASEFOLD
#define	FNM_FILE_NAME	FNM_PATHNAME

/*
 * Our data structures.
 */

typedef struct named_rr nrr_t;
typedef DLZ_LIST(nrr_t) rr_list_t;

typedef struct config_data {
	char		*zone_pattern;
	char		*axfr_pattern;
	rr_list_t	rrs_list;
	char		*zone;
	char		*record;
	char		*client;

	/* Helper functions from the dlz_dlopen driver */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;
} config_data_t;

struct named_rr {
	char		*name;
	char		*type;
	int		ttl;
	query_list_t	*data;
	DLZ_LINK(nrr_t)	link;
};

/*
 * Forward references
 */
static int
rangematch(const char *, char, int, char **);

static int
fnmatch(const char *pattern, const char *string, int flags);

static void
b9_add_helper(struct config_data *cd, const char *helper_name, void *ptr);

static const char *
shortest_match(const char *pattern, const char *string);

isc_result_t
dlz_allnodes(const char *zone, void *dbdata, dns_sdlzallnodes_t *allnodes) {
	config_data_t *cd = (config_data_t *) dbdata;
	isc_result_t result;
	char *querystring = NULL;
	nrr_t *nrec;
	int i = 0;

	DE_CONST(zone, cd->zone);

	/* Write info message to log */
	cd->log(ISC_LOG_DEBUG(1),
		"dlz_wildcard allnodes called for zone '%s'", zone);

	result = ISC_R_FAILURE;

	nrec = DLZ_LIST_HEAD(cd->rrs_list);
	while (nrec != NULL) {
		cd->record = nrec->name;

		querystring = build_querystring(nrec->data);

		if (querystring == NULL) {
			result = ISC_R_NOMEMORY;
			goto done;
		}

		cd->log(ISC_LOG_DEBUG(2),
			"dlz_wildcard allnodes entry num %d: calling "
			"putnamedrr(name=%s type=%s ttl=%d qs=%s)",
			i++, nrec->name, nrec->type, nrec->ttl, querystring);

		result = cd->putnamedrr(allnodes, nrec->name, nrec->type,
					nrec->ttl, querystring);
		if (result != ISC_R_SUCCESS)
			goto done;

		nrec = DLZ_LIST_NEXT(nrec, link);
	}

done:
	cd->zone = NULL;

	if (querystring != NULL)
		free(querystring);

	return (result);
}

isc_result_t
dlz_allowzonexfr(void *dbdata, const char *name, const char *client) {
	config_data_t *cd = (config_data_t *) dbdata;

	UNUSED(name);

	/* Write info message to log */
	cd->log(ISC_LOG_DEBUG(1),
		"dlz_wildcard allowzonexfr called for client '%s'", client);

	if (fnmatch(cd->axfr_pattern, client, FNM_CASEFOLD) == 0)
		return (ISC_R_SUCCESS);
	else
		return (ISC_R_NOTFOUND);
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
	config_data_t *cd = (config_data_t *) dbdata;
	const char *p;

#if DLZ_DLOPEN_VERSION >= 3
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	p = shortest_match(cd->zone_pattern, name);
	if (p == NULL)
		return (ISC_R_NOTFOUND);

	/* Write info message to log */
	cd->log(ISC_LOG_DEBUG(1),
		"dlz_wildcard findzonedb matched '%s'", p);

	return (ISC_R_SUCCESS);
}

#if DLZ_DLOPEN_VERSION == 1
isc_result_t
dlz_lookup(const char *zone, const char *name,
	   void *dbdata, dns_sdlzlookup_t *lookup)
#else
isc_result_t
dlz_lookup(const char *zone, const char *name,
	   void *dbdata, dns_sdlzlookup_t *lookup,
	   dns_clientinfomethods_t *methods,
	   dns_clientinfo_t *clientinfo)
#endif
{
	isc_result_t result;
	config_data_t *cd = (config_data_t *) dbdata;
	char *querystring = NULL;
	const char *p;
	char *namebuf;
	nrr_t *nrec;
	isc_boolean_t origin = ISC_TRUE;

#if DLZ_DLOPEN_VERSION >= 2
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	p = shortest_match(cd->zone_pattern, zone);
	if (p == NULL)
		return (ISC_R_NOTFOUND);

	DE_CONST(name, cd->record);
	DE_CONST(p, cd->zone);

	if ((p != zone) && (strcmp(name, "@") == 0 || strcmp(name, zone) == 0))
	{
		size_t len = p - zone;
		namebuf = malloc(len);
		strncpy(namebuf, zone, len - 1);
		namebuf[len - 1] = '\0';
		cd->record = namebuf;
		origin = ISC_FALSE;
	} else if (p == zone)
		cd->record = "@";

	/* Write info message to log */
	cd->log(ISC_LOG_DEBUG(1),
		"dlz_wildcard_dynamic: lookup for '%s' in '%s': "
		"trying '%s' in '%s'",
		name, zone, cd->record, cd->zone);

	result = ISC_R_NOTFOUND;
	nrec = DLZ_LIST_HEAD(cd->rrs_list);
	while (nrec != NULL) {
		nrr_t *next = DLZ_LIST_NEXT(nrec, link);
		if (strcmp(cd->record, nrec->name) == 0) {
			/* We handle authority data in dlz_authority() */
			if (strcmp(nrec->type, "SOA") == 0 ||
			    strcmp(nrec->type, "NS") == 0)
			{
				nrec = next;
				continue;
			}

			querystring = build_querystring(nrec->data);
			if (querystring == NULL) {
				result = ISC_R_NOMEMORY;
				goto done;
			}

			result = cd->putrr(lookup, nrec->type,
					   nrec->ttl, querystring);
			if (result != ISC_R_SUCCESS)
				goto done;

			result = ISC_R_SUCCESS;

			free(querystring);
			querystring = NULL;
		}
		nrec = next;
	}

done:
	cd->zone = NULL;
	cd->record = NULL;

	if (querystring != NULL)
		free(querystring);

	return (result);
}

isc_result_t
dlz_authority(const char *zone, void *dbdata, dns_sdlzlookup_t *lookup) {
	isc_result_t result;
	config_data_t *cd = (config_data_t *) dbdata;
	char *querystring = NULL;
	nrr_t *nrec;
	const char *p, *name = "@";

	p = shortest_match(cd->zone_pattern, zone);
	if (p == NULL)
		return (ISC_R_NOTFOUND);

	DE_CONST(p, cd->zone);

	/* Write info message to log */
	cd->log(ISC_LOG_DEBUG(1),
		"dlz_wildcard_dynamic: authority for '%s'", zone);

	result = ISC_R_NOTFOUND;
	nrec = DLZ_LIST_HEAD(cd->rrs_list);
	while (nrec != NULL) {
		isc_boolean_t origin;
		if (strcmp("@", nrec->name) == 0) {
			isc_result_t presult;

			querystring = build_querystring(nrec->data);
			if (querystring == NULL) {
				result = ISC_R_NOMEMORY;
				goto done;
			}

			presult = cd->putrr(lookup, nrec->type,
					   nrec->ttl, querystring);
			if (presult != ISC_R_SUCCESS) {
				result = presult;
				goto done;
			}

			result = ISC_R_SUCCESS;

			free(querystring);
			querystring = NULL;
		}
		nrec = DLZ_LIST_NEXT(nrec, link);
	}

done:
	cd->zone = NULL;

	if (querystring != NULL)
		free(querystring);

	return (result);
}

static void
destroy_rrlist(config_data_t *cd) {
	nrr_t *trec, *nrec;

	nrec = DLZ_LIST_HEAD(cd->rrs_list);

	while (nrec != NULL) {
		trec = nrec;

		destroy_querylist(&trec->data);

		if (trec->name != NULL)
			free(trec->name);
		if (trec->type != NULL)
			free(trec->type);
		trec->name = trec->type = NULL;

		/* Get the next record, before we destroy this one. */
		nrec = DLZ_LIST_NEXT(nrec, link);

		free(trec);
	}
}

isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[],
	   void **dbdata, ...)
{
	config_data_t *cd;
	char *endp;
	int i, def_ttl;
	nrr_t *trec = NULL;
	isc_result_t result;
	const char *helper_name;
	va_list ap;

	if (argc < 8 || argc % 4 != 0)
		return (ISC_R_FAILURE);

	cd = calloc(1, sizeof(config_data_t));
	if (cd == NULL)
		return (ISC_R_NOMEMORY);
	memset(cd, 0, sizeof(config_data_t));

	/* Fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char*)) != NULL)
		b9_add_helper(cd, helper_name, va_arg(ap, void*));
	va_end(ap);

	/*
	 * Write info message to log
	 */
	cd->log(ISC_LOG_INFO,
		"Loading '%s' using DLZ_wildcard driver. "
		"Zone: %s, AXFR allowed for: %s, $TTL: %s",
		dlzname, argv[1], argv[2], argv[3]);

	/* initialize the records list here to simplify cleanup */
	DLZ_LIST_INIT(cd->rrs_list);

	cd->zone_pattern = strdup(argv[1]);
	if (cd->zone_pattern == NULL)
		goto cleanup;

	cd->axfr_pattern = strdup(argv[2]);
	if (cd->axfr_pattern == NULL)
		goto cleanup;

	def_ttl = strtol(argv[3], &endp, 10);
	if (*endp != '\0' || def_ttl < 0) {
		def_ttl = 3600;
		cd->log(ISC_LOG_ERROR, "default TTL invalid, using 3600");
	}

	for (i = 4; i < argc; i += 4) {
		result = ISC_R_NOMEMORY;

		trec = malloc(sizeof(nrr_t));
		if (trec == NULL)
			goto full_cleanup;

		memset(trec, 0, sizeof(nrr_t));

		/* Initialize the record link */
		DLZ_LINK_INIT(trec, link);
		/* Append the record to the list */
		DLZ_LIST_APPEND(cd->rrs_list, trec, link);

		trec->name = strdup(argv[i]);
		if (trec->name == NULL)
			goto full_cleanup;

		trec->type = strdup(argv[i + 2]);
		if (trec->type == NULL)
			goto full_cleanup;

		trec->ttl = strtol(argv[i + 1], &endp, 10);
		if (argv[i + 1][0] == '\0' || *endp != '\0' || trec->ttl < 0)
			trec->ttl = def_ttl;

		result = build_querylist(argv[i + 3], &cd->zone,
					 &cd->record, &cd->client,
					 &trec->data, 0, cd->log);
		/* If unsuccessful, log err msg and cleanup */
		if (result != ISC_R_SUCCESS) {
			cd->log(ISC_LOG_ERROR,
				"Could not build RR data list at argv[%d]",
				i + 3);
			goto full_cleanup;
		}
	}

	*dbdata = cd;

	return (ISC_R_SUCCESS);

full_cleanup:
	destroy_rrlist(cd);

cleanup:
	if (cd->zone_pattern != NULL)
		free(cd->zone_pattern);
	if (cd->axfr_pattern != NULL)
		free(cd->axfr_pattern);
	free(cd);

	return (result);
}

void
dlz_destroy(void *dbdata) {
	config_data_t *cd = (config_data_t *) dbdata;

	/*
	 * Write debugging message to log
	 */
	cd->log(ISC_LOG_DEBUG(2), "Unloading DLZ_wildcard driver.");

	destroy_rrlist(cd);

	free(cd->zone_pattern);
	free(cd->axfr_pattern);
	free(cd);
}


/*
 * Return the version of the API
 */
int
dlz_version(unsigned int *flags) {
	UNUSED(flags);
	/* XXX: ok to set DNS_SDLZFLAG_THREADSAFE here? */
	return (DLZ_DLOPEN_VERSION);
}

/*
 * Register a helper function from the bind9 dlz_dlopen driver
 */
static void
b9_add_helper(struct config_data *cd, const char *helper_name, void *ptr) {
	if (strcmp(helper_name, "log") == 0)
		cd->log = (log_t *)ptr;
	if (strcmp(helper_name, "putrr") == 0)
		cd->putrr = (dns_sdlz_putrr_t *)ptr;
	if (strcmp(helper_name, "putnamedrr") == 0)
		cd->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
	if (strcmp(helper_name, "writeable_zone") == 0)
		cd->writeable_zone = (dns_dlz_writeablezone_t *)ptr;
}

static const char *
shortest_match(const char *pattern, const char *string) {
	const char *p = string;
	if (pattern == NULL || p == NULL || *p == '\0')
		return (NULL);

	p += strlen(p);
	while (p-- > string) {
		if (*p == '.') {
			if (fnmatch(pattern, p + 1, FNM_CASEFOLD) == 0)
				return (p + 1);
		}
	}
	if (fnmatch(pattern, string, FNM_CASEFOLD) == 0)
		return (string);

	return (NULL);
}

/*
 * The helper functions stolen from the FreeBSD kernel (sys/libkern/fnmatch.c).
 *
 * Why don't we use fnmatch(3) from libc? Because it is not thread-safe, and
 * it is not thread-safe because it supports multibyte characters. But here,
 * in BIND, we want to be thread-safe and don't need multibyte - DNS names are
 * always ASCII.
 */
#define	EOS	'\0'

#define RANGE_MATCH     1
#define RANGE_NOMATCH   0
#define RANGE_ERROR     (-1)

static int
fnmatch(const char *pattern, const char *string, int flags) {
	const char *stringstart;
	char *newp;
	char c, test;

	for (stringstart = string;;)
		switch (c = *pattern++) {
		case EOS:
			if ((flags & FNM_LEADING_DIR) && *string == '/')
				return (0);
			return (*string == EOS ? 0 : FNM_NOMATCH);
		case '?':
			if (*string == EOS)
				return (FNM_NOMATCH);
			if (*string == '/' && (flags & FNM_PATHNAME))
				return (FNM_NOMATCH);
			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);
			++string;
			break;
		case '*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == '*')
				c = *++pattern;

			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);

			/* Optimize for pattern with * at end or before /. */
			if (c == EOS)
				if (flags & FNM_PATHNAME)
					return ((flags & FNM_LEADING_DIR) ||
					    index(string, '/') == NULL ?
					    0 : FNM_NOMATCH);
				else
					return (0);
			else if (c == '/' && flags & FNM_PATHNAME) {
				if ((string = index(string, '/')) == NULL)
					return (FNM_NOMATCH);
				break;
			}

			/* General case, use recursion. */
			while ((test = *string) != EOS) {
				if (!fnmatch(pattern, string,
					     flags & ~FNM_PERIOD))
					return (0);
				if (test == '/' && flags & FNM_PATHNAME)
					break;
				++string;
			}
			return (FNM_NOMATCH);
		case '[':
			if (*string == EOS)
				return (FNM_NOMATCH);
			if (*string == '/' && (flags & FNM_PATHNAME))
				return (FNM_NOMATCH);
			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);

			switch (rangematch(pattern, *string, flags, &newp)) {
			case RANGE_ERROR:
				goto norm;
			case RANGE_MATCH:
				pattern = newp;
				break;
			case RANGE_NOMATCH:
				return (FNM_NOMATCH);
			}
			++string;
			break;
		case '\\':
			if (!(flags & FNM_NOESCAPE)) {
				if ((c = *pattern++) == EOS) {
					c = '\\';
					--pattern;
				}
			}
			/* FALLTHROUGH */
		default:
		norm:
			if (c == *string)
				;
			else if ((flags & FNM_CASEFOLD) &&
				 (tolower((unsigned char)c) ==
				  tolower((unsigned char)*string)))
				;
			else
				return (FNM_NOMATCH);
			string++;
			break;
		}
	/* NOTREACHED */
}

static int
rangematch(const char *pattern, char test, int flags, char **newp) {
	int negate, ok;
	char c, c2;

	/*
	 * A bracket expression starting with an unquoted circumflex
	 * character produces unspecified results (IEEE 1003.2-1992,
	 * 3.13.2).  This implementation treats it like '!', for
	 * consistency with the regular expression syntax.
	 * J.T. Conklin (conklin@ngai.kaleida.com)
	 */
	if ( (negate = (*pattern == '!' || *pattern == '^')) )
		++pattern;

	if (flags & FNM_CASEFOLD)
		test = tolower((unsigned char)test);

	/*
	 * A right bracket shall lose its special meaning and represent
	 * itself in a bracket expression if it occurs first in the list.
	 * -- POSIX.2 2.8.3.2
	 */
	ok = 0;
	c = *pattern++;
	do {
		if (c == '\\' && !(flags & FNM_NOESCAPE))
			c = *pattern++;
		if (c == EOS)
			return (RANGE_ERROR);

		if (c == '/' && (flags & FNM_PATHNAME))
			return (RANGE_NOMATCH);

		if (flags & FNM_CASEFOLD)
			c = tolower((unsigned char)c);

		if (*pattern == '-'
		    && (c2 = *(pattern+1)) != EOS && c2 != ']') {
			pattern += 2;
			if (c2 == '\\' && !(flags & FNM_NOESCAPE))
				c2 = *pattern++;
			if (c2 == EOS)
				return (RANGE_ERROR);

			if (flags & FNM_CASEFOLD)
				c2 = tolower((unsigned char)c2);

			if (c <= test && test <= c2)
				ok = 1;
		} else if (c == test)
			ok = 1;
	} while ((c = *pattern++) != ']');

	*newp = (char *)(uintptr_t)pattern;
	return (ok == negate ? RANGE_NOMATCH : RANGE_MATCH);
}
