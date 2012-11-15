/*	$NetBSD: getnetgrent.c,v 1.42 2012/03/20 16:36:05 matt Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getnetgrent.c,v 1.42 2012/03/20 16:36:05 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <db.h>
#include <err.h>
#include <fcntl.h>
#define _NETGROUP_PRIVATE
#include <stringlist.h>
#include <netgroup.h>
#include <nsswitch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#endif

#ifdef __weak_alias
__weak_alias(endnetgrent,_endnetgrent)
__weak_alias(getnetgrent,_getnetgrent)
__weak_alias(innetgr,_innetgr)
__weak_alias(setnetgrent,_setnetgrent)
#endif

#define _NG_STAR(s)	(((s) == NULL || *(s) == '\0') ? _ngstar : s)
#define _NG_EMPTY(s)	((s) == NULL ? "" : s)
#define _NG_ISSPACE(p)	(isspace((unsigned char) (p)) || (p) == '\n')

static const char _ngstar[] = "*";
static struct netgroup *_nghead = NULL;
static struct netgroup *_nglist = NULL;
static DB *_ng_db;

static int getstring(char **, int, __aconst char **);
static struct netgroup *getnetgroup(char **);
static int lookup(char *, char **, int);
static int addgroup(StringList *, char *);
static int in_check(const char *, const char *, const char *,
    struct netgroup *);
static int in_find(StringList *, char *, const char *, const char *,
    const char *);
static char *in_lookup1(const char *, const char *, int);
static int in_lookup(const char *, const char *, const char *, int);

#ifdef NSSRC_FILES
static const ns_src default_files_nis[] = {
	{ NSSRC_FILES,	NS_SUCCESS | NS_NOTFOUND },
#ifdef YP
	{ NSSRC_NIS,	NS_SUCCESS },
#endif
	{ 0, 0 },
};
#endif

/*
 * getstring(): Get a string delimited by the character, skipping leading and
 * trailing blanks and advancing the pointer
 */
static int
getstring(char **pp, int del, char __aconst **str)
{
	size_t len;
	char *sp, *ep, *dp;

	_DIAGASSERT(pp != NULL);
	_DIAGASSERT(str != NULL);

	/* skip leading blanks */
	for (sp = *pp; *sp && _NG_ISSPACE(*sp); sp++)
		continue;

	/* accumulate till delimiter or space */
	for (ep = sp; *ep && *ep != del && !_NG_ISSPACE(*ep); ep++)
		continue;

	/* hunt for the delimiter */
	for (dp = ep; *dp && *dp != del && _NG_ISSPACE(*dp); dp++)
		continue;

	if (*dp != del) {
		*str = NULL;
		return 0;
	}

	*pp = ++dp;

	len = (ep - sp) + 1;
	if (len > 1) {
		dp = malloc(len);
		if (dp == NULL)
			return 0;
		(void)memcpy(dp, sp, len);
		dp[len - 1] = '\0';
	} else
		dp = NULL;

	*str = dp;
	return 1;
}


/*
 * getnetgroup(): Parse a netgroup, and advance the pointer
 */
static struct netgroup *
getnetgroup(char **pp)
{
	struct netgroup *ng;

	_DIAGASSERT(pp != NULL);
	_DIAGASSERT(*pp != NULL);

	ng = malloc(sizeof(struct netgroup));
	if (ng == NULL)
		return NULL;

	(*pp)++;	/* skip '(' */
	if (!getstring(pp, ',', &ng->ng_host))
		goto badhost;

	if (!getstring(pp, ',', &ng->ng_user))
		goto baduser;

	if (!getstring(pp, ')', &ng->ng_domain))
		goto baddomain;

#ifdef DEBUG_NG
	{
		char buf[1024];
		(void) fprintf(stderr, "netgroup %s\n",
		    _ng_print(buf, sizeof(buf), ng));
	}
#endif
	return ng;

baddomain:
	if (ng->ng_user)
		free(ng->ng_user);
baduser:
	if (ng->ng_host)
		free(ng->ng_host);
badhost:
	free(ng);
	return NULL;
}

void
_ng_cycle(const char *grp, const StringList *sl)
{
	size_t i;
	warnx("netgroup: Cycle in group `%s'", grp);
	(void)fprintf(stderr, "groups: ");
	for (i = 0; i < sl->sl_cur; i++)
		(void)fprintf(stderr, "%s ", sl->sl_str[i]);
	(void)fprintf(stderr, "\n");
}

static int _local_lookup(void *, void *, va_list);

/*ARGSUSED*/
static int
_local_lookup(void *rv, void *cb_data, va_list ap)
{
	char	 *name = va_arg(ap, char *);
	char	**line = va_arg(ap, char **);
	int	  bywhat = va_arg(ap, int);

	DBT	 key, data;
	size_t	 len;
	char	*ks;
	int	 r;

	if (_ng_db == NULL)
		return NS_UNAVAIL;

	len = strlen(name) + 2;
	ks = malloc(len);
	if (ks == NULL)
		return NS_UNAVAIL;

	ks[0] = bywhat;
	(void)memcpy(&ks[1], name, len - 1);

	key.data = (u_char *)ks;
	key.size = len;

	r = (*_ng_db->get)(_ng_db, &key, &data, 0);
	free(ks);
	switch (r) {
	case 0:
		break;
	case 1:
		return NS_NOTFOUND;
	case -1:
			/* XXX: call endnetgrent() here ? */
		return NS_UNAVAIL;
	}

	*line = strdup(data.data);
	if (*line == NULL)
		return NS_UNAVAIL;
	return NS_SUCCESS;
}

#ifdef YP
static int _nis_lookup(void *, void *, va_list);

/*ARGSUSED*/
static int
_nis_lookup(void *rv, void *cb_data, va_list ap)
{
	char	 *name = va_arg(ap, char *);
	char	**line = va_arg(ap, char **);
	int	  bywhat = va_arg(ap, int);

	static char	*__ypdomain;
	int              i;
	const char      *map = NULL;

	if(__ypdomain == NULL) {
		switch (yp_get_default_domain(&__ypdomain)) {
		case 0:
			break;
		case YPERR_RESRC:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	}

	switch (bywhat) {
	case _NG_KEYBYNAME:
		map = "netgroup";
		break;

	case _NG_KEYBYUSER:
		map = "netgroup.byuser";
		break;

	case _NG_KEYBYHOST:
		map = "netgroup.byhost";
		break;

	default:
		abort();
	}

	*line = NULL;
	switch (yp_match(__ypdomain, map, name, (int)strlen(name), line, &i)) {
	case 0:
		return NS_SUCCESS;
	case YPERR_KEY:
		if (*line)
			free(*line);
		return NS_NOTFOUND;
	default:
		if (*line)
			free(*line);
		return NS_UNAVAIL;
	}
	/* NOTREACHED */
}
#endif

#ifdef NSSRC_FILES
/*
 * lookup(): Find the given key in the database or yp, and return its value
 * in *line; returns 1 if key was found, 0 otherwise
 */
static int
lookup(char *name, char	**line, int bywhat)
{
	int		r;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_lookup, NULL)
		NS_NIS_CB(_nis_lookup, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(name != NULL);
	_DIAGASSERT(line != NULL);

	r = nsdispatch(NULL, dtab, NSDB_NETGROUP, "lookup", default_files_nis,
	    name, line, bywhat);
	return (r == NS_SUCCESS) ? 1 : 0;
}
#else
static int
_local_lookupv(int *rv, void *cbdata, ...)
{
	int e;
	va_list ap;
	va_start(ap, cbdata);
	e = _local_lookup(rv, cbdata, ap);
	va_end(ap);
	return e;
}

static int
lookup(name, line, bywhat)
	char	 *name;
	char	**line;
	int	  bywhat;
{
	return _local_lookupv(NULL, NULL, name, line, bywhat) == NS_SUCCESS;
}
#endif

/*
 * _ng_parse(): Parse a line and return: _NG_ERROR: Syntax Error _NG_NONE:
 * line was empty or a comment _NG_GROUP: line had a netgroup definition,
 * returned in ng _NG_NAME:  line had a netgroup name, returned in name
 * 
 * Public since used by netgroup_mkdb
 */
int
_ng_parse(char **p, char **name, struct netgroup **ng)
{

	_DIAGASSERT(p != NULL);
	_DIAGASSERT(*p != NULL);
	_DIAGASSERT(name != NULL);
	_DIAGASSERT(ng != NULL);

	while (**p) {
		if (**p == '#')
			/* comment */
			return _NG_NONE;

		while (**p && _NG_ISSPACE(**p))
			/* skipblank */
			(*p)++;

		if (**p == '(') {
			if ((*ng = getnetgroup(p)) == NULL)
				return _NG_ERROR;
			return _NG_GROUP;
		} else {
			char	*np;
			size_t	i;

			for (np = *p; **p && !_NG_ISSPACE(**p); (*p)++)
				continue;
			if (np != *p) {
				i = (*p - np) + 1;
				*name = malloc(i);
				if (*name == NULL)
					return _NG_ERROR;
				(void)memcpy(*name, np, i);
				(*name)[i - 1] = '\0';
				return _NG_NAME;
			}
		}
	}
	return _NG_NONE;
}


/*
 * addgroup(): Recursively add all the members of the netgroup to this group.
 * returns 0 upon failure, nonzero upon success.
 * grp is not a valid pointer after return (either free(3)ed or allocated
 * to a stringlist). in either case, it shouldn't be used again.
 */
static int
addgroup(StringList *sl, char *grp)
{
	char		*line, *p;
	struct netgroup	*ng;
	char		*name;

	_DIAGASSERT(sl != NULL);
	_DIAGASSERT(grp != NULL);

#ifdef DEBUG_NG
	(void)fprintf(stderr, "addgroup(%s)\n", grp);
#endif
	/* check for cycles */
	if (sl_find(sl, grp) != NULL) {
		_ng_cycle(grp, sl);
		free(grp);
		return 0;
	}
	if (sl_add(sl, grp) == -1) {
		free(grp);
		return 0;
	}

	/* Lookup this netgroup */
	line = NULL;
	if (!lookup(grp, &line, _NG_KEYBYNAME)) {
		if (line)
			free(line);
		return 0;
	}

	p = line;

	for (;;) {
		switch (_ng_parse(&p, &name, &ng)) {
		case _NG_NONE:
			/* Done with the line */
			free(line);
			return 1;

		case _NG_GROUP:
			/* new netgroup */
			/* add to the list */
			ng->ng_next = _nglist;
			_nglist = ng;
			break;

		case _NG_NAME:
			/* netgroup name */
			if (!addgroup(sl, name))
				return 0;
			break;

		case _NG_ERROR:
			return 0;

		default:
			abort();
		}
	}
}


/*
 * in_check(): Compare the spec with the netgroup
 */
static int
in_check(const char *host, const char *user, const char *domain,
    struct netgroup *ng)
{

	/* host may be NULL */
	/* user may be NULL */
	/* domain may be NULL */
	_DIAGASSERT(ng != NULL);

	if ((host != NULL) && (ng->ng_host != NULL)
	    && strcmp(ng->ng_host, host) != 0)
		return 0;

	if ((user != NULL) && (ng->ng_user != NULL)
	    && strcmp(ng->ng_user, user) != 0)
		return 0;

	if ((domain != NULL) && (ng->ng_domain != NULL)
	    && strcmp(ng->ng_domain, domain) != 0)
		return 0;

	return 1;
}


/*
 * in_find(): Find a match for the host, user, domain spec.
 * grp is not a valid pointer after return (either free(3)ed or allocated
 * to a stringlist). in either case, it shouldn't be used again.
 */
static int
in_find(StringList *sl, char *grp, const char *host, const char *user,
    const char *domain)
{
	char		*line, *p;
	int		 i;
	struct netgroup	*ng;
	char		*name;

	_DIAGASSERT(sl != NULL);
	_DIAGASSERT(grp != NULL);
	/* host may be NULL */
	/* user may be NULL */
	/* domain may be NULL */

#ifdef DEBUG_NG
	(void)fprintf(stderr, "in_find(%s)\n", grp);
#endif
	/* check for cycles */
	if (sl_find(sl, grp) != NULL) {
		_ng_cycle(grp, sl);
		free(grp);
		return 0;
	}
	if (sl_add(sl, grp) == -1) {
		free(grp);
		return 0;
	}

	/* Lookup this netgroup */
	line = NULL;
	if (!lookup(grp, &line, _NG_KEYBYNAME)) {
		if (line)
			free(line);
		return 0;
	}

	p = line;

	for (;;) {
		switch (_ng_parse(&p, &name, &ng)) {
		case _NG_NONE:
			/* Done with the line */
			free(line);
			return 0;

		case _NG_GROUP:
			/* new netgroup */
			i = in_check(host, user, domain, ng);
			if (ng->ng_host != NULL)
				free(ng->ng_host);
			if (ng->ng_user != NULL)
				free(ng->ng_user);
			if (ng->ng_domain != NULL)
				free(ng->ng_domain);
			free(ng);
			if (i) {
				free(line);
				return 1;
			}
			break;

		case _NG_NAME:
			/* netgroup name */
			if (in_find(sl, name, host, user, domain)) {
				free(line);
				return 1;
			}
			break;

		case _NG_ERROR:
			free(line);
			return 0;

		default:
			abort();
		}
	}
}

/*
 * _ng_makekey(): Make a key from the two names given. The key is of the form
 * <name1>.<name2> Names strings are replaced with * if they are empty;
 * Returns NULL if there's a problem.
 */
char *
_ng_makekey(const char *s1, const char *s2, size_t len)
{
	char *buf;

	/* s1 may be NULL */
	/* s2 may be NULL */

	buf = malloc(len);
	if (buf != NULL)
		(void)snprintf(buf, len, "%s.%s", _NG_STAR(s1), _NG_STAR(s2));
	return buf;
}

void
_ng_print(char *buf, size_t len, const struct netgroup *ng)
{
	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(ng != NULL);

	(void)snprintf(buf, len, "(%s,%s,%s)", _NG_EMPTY(ng->ng_host),
	    _NG_EMPTY(ng->ng_user), _NG_EMPTY(ng->ng_domain));
}


/*
 * in_lookup1(): Fast lookup for a key in the appropriate map
 */
static char *
in_lookup1(const char *key, const char *domain, int map)
{
	char	*line;
	size_t	 len;
	char	*ptr;
	int	 res;

	/* key may be NULL */
	/* domain may be NULL */

	len = (key ? strlen(key) : 1) + (domain ? strlen(domain) : 1) + 2;
	ptr = _ng_makekey(key, domain, len);
	if (ptr == NULL)
		return NULL;
	res = lookup(ptr, &line, map);
	free(ptr);
	return res ? line : NULL;
}


/*
 * in_lookup(): Fast lookup for a key in the appropriate map
 */
static int
in_lookup(const char *group, const char *key, const char *domain, int map)
{
	size_t	 len;
	char	*ptr, *line;

	_DIAGASSERT(group != NULL);
	/* key may be NULL */
	/* domain may be NULL */

	if (domain != NULL) {
		/* Domain specified; look in "group.domain" and "*.domain" */
		if ((line = in_lookup1(key, domain, map)) == NULL)
			line = in_lookup1(NULL, domain, map);
	} else 
		line = NULL;

	if (line == NULL) {
	    /* 
	     * domain not specified or domain lookup failed; look in
	     * "group.*" and "*.*"
	     */
	    if (((line = in_lookup1(key, NULL, map)) == NULL) &&
		((line = in_lookup1(NULL, NULL, map)) == NULL))
		return 0;
	}

	len = strlen(group);

	for (ptr = line; (ptr = strstr(ptr, group)) != NULL;)
		/* Make sure we did not find a substring */
		if ((ptr != line && ptr[-1] != ',') ||
		    (ptr[len] != '\0' && strchr("\n\t ,", ptr[len]) == NULL))
			ptr++;
		else {
			free(line);
			return 1;
		}

	free(line);
	return 0;
}

/*ARGSUSED*/
static int
_local_endnetgrent(void *rv, void *cb_data, va_list ap)
{
	for (_nglist = _nghead; _nglist != NULL; _nglist = _nghead) {
		_nghead = _nglist->ng_next;
		if (_nglist->ng_host != NULL)
			free(_nglist->ng_host);
		if (_nglist->ng_user != NULL)
			free(_nglist->ng_user);
		if (_nglist->ng_domain != NULL)
			free(_nglist->ng_domain);
		free(_nglist);
	}

	if (_ng_db) {
		(void)(*_ng_db->close)(_ng_db);
		_ng_db = NULL;
	}

	return NS_SUCCESS;
}

/*ARGSUSED*/
static int
_local_setnetgrent(void *rv, void *cb_data, va_list ap)
{
	const char	*ng = va_arg(ap, const char *);
	StringList	*sl;
	char		*ng_copy;

	_DIAGASSERT(ng != NULL);

	sl = sl_init();
	if (sl == NULL)
		return NS_TRYAGAIN;

	/* Cleanup any previous storage */
	if (_nghead != NULL)
		endnetgrent();

	if (_ng_db == NULL)
		_ng_db = dbopen(_PATH_NETGROUP_DB, O_RDONLY, 0, DB_HASH, NULL);

	ng_copy = strdup(ng);
	if (ng_copy != NULL)
		addgroup(sl, ng_copy);
	_nghead = _nglist;
	sl_free(sl, 1);

	return NS_SUCCESS;
}

/*ARGSUSED*/
static int
_local_getnetgrent(void *rv, void *cb_data, va_list ap)
{
	int *retval = va_arg(ap, int *);
	const char **host = va_arg(ap, const char **);
	const char **user = va_arg(ap, const char **);
	const char **domain = va_arg(ap, const char **);

	_DIAGASSERT(host != NULL);
	_DIAGASSERT(user != NULL);
	_DIAGASSERT(domain != NULL);

	*retval = 0;

	if (_nglist == NULL)
		return NS_TRYAGAIN;

	*host   = _nglist->ng_host;
	*user   = _nglist->ng_user;
	*domain = _nglist->ng_domain;

	_nglist = _nglist->ng_next;

	*retval = 1;

	return NS_SUCCESS;
}

/*ARGSUSED*/
static int
_local_innetgr(void *rv, void *cb_data, va_list ap)
{
	int *retval = va_arg(ap, int *);
	const char *grp = va_arg(ap, const char *);
	const char *host = va_arg(ap, const char *);
	const char *user = va_arg(ap, const char *);
	const char *domain = va_arg(ap, const char *);

	int	 found;
	StringList *sl;
	char *grcpy;

	_DIAGASSERT(grp != NULL);
	/* host may be NULL */
	/* user may be NULL */
	/* domain may be NULL */

	if (_ng_db == NULL)
		_ng_db = dbopen(_PATH_NETGROUP_DB, O_RDONLY, 0, DB_HASH, NULL);

	/* Try the fast lookup first */
	if (host != NULL && user == NULL) {
		if (in_lookup(grp, host, domain, _NG_KEYBYHOST)) {
			*retval = 1;
			return NS_SUCCESS;
		}
	} else if (host == NULL && user != NULL) {
		if (in_lookup(grp, user, domain, _NG_KEYBYUSER)) {
			*retval = 1;
			return NS_SUCCESS;
		}
	}
	/* If a domainname is given, we would have found a match */
	if (domain != NULL) {
		*retval = 0;
		return NS_SUCCESS;
	}

	/* Too bad need the slow recursive way */
	sl = sl_init();
	if (sl == NULL) {
		*retval = 0;
		return NS_SUCCESS;
	}
	if ((grcpy = strdup(grp)) == NULL) {
		sl_free(sl, 1);
		*retval = 0;
		return NS_SUCCESS;
	}
	found = in_find(sl, grcpy, host, user, domain);
	sl_free(sl, 1);

	*retval = found;
	return NS_SUCCESS;
}

#ifdef YP

/*ARGSUSED*/
static int
_nis_endnetgrent(void *rv, void *cb_data, va_list ap)
{
	return _local_endnetgrent(rv, cb_data, ap);
}

/*ARGSUSED*/
static int
_nis_setnetgrent(void *rv, void *cb_data, va_list ap)
{
	return _local_setnetgrent(rv, cb_data, ap);
}

/*ARGSUSED*/
static int
_nis_getnetgrent(void *rv, void *cb_data, va_list ap)
{
	return _local_getnetgrent(rv, cb_data, ap);
}

/*ARGSUSED*/
static int
_nis_innetgr(void *rv, void *cb_data, va_list ap)
{
	return _local_innetgr(rv, cb_data, ap);
}

#endif


#ifdef NSSRC_FILES
void
endnetgrent(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_endnetgrent, NULL)
		NS_NIS_CB(_nis_endnetgrent, NULL)
		NS_NULL_CB
	};

	(void) nsdispatch(NULL, dtab, NSDB_NETGROUP, "endnetgrent",
			  __nsdefaultcompat);
}
#else
static int
_local_endnetgrentv(int *rv, void *cbdata, ...)
{
	int e;
	va_list ap;
	va_start(ap, cbdata);
	e = _local_endnetgrent(rv, cbdata, ap);
	va_end(ap);
	return e;
}

void
endnetgrent(void)
{
	(void)_local_endnetgrentv(NULL, NULL, NULL);
}
#endif

#ifdef NSSRC_FILES
void
setnetgrent(const char *ng)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_setnetgrent, NULL)
		NS_NIS_CB(_nis_setnetgrent, NULL)
		NS_NULL_CB
	};

	(void) nsdispatch(NULL, dtab, NSDB_NETGROUP, "setnetgrent",
			   __nsdefaultnis, ng);
}
#else
static int
_local_setnetgrentv(int *rv, void *cbdata, ...)
{
	int e;
	va_list ap;
	va_start(ap, cbdata);
	e = _local_setnetgrent(rv, cbdata, ap);
	va_end(ap);
	return e;
}

void
setnetgrent(const char *ng)
{
	(void) _local_setnetgrentv(NULL, NULL,ng);
}

#endif

#ifdef NSSRC_FILES
int
getnetgrent(const char **host, const char **user, const char **domain)
{
	int     r, retval;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_getnetgrent, NULL)
		NS_NIS_CB(_nis_getnetgrent, NULL)
		NS_NULL_CB
	};

	r = nsdispatch(NULL, dtab, NSDB_NETGROUP, "getnetgrent",
		       __nsdefaultnis, &retval, host, user, domain);

	return (r == NS_SUCCESS) ? retval : 0;
}
#else
static int
_local_getnetgrentv(int *rv, void *cbdata, ...)
{
	int e;
	va_list ap;
	va_start(ap, cbdata);
	e = _local_getnetgrent(rv, cbdata, ap);
	va_end(ap);
	return e;
}

int
getnetgrent(const char **host, const char **user, const char **domain)
{
	return _local_getnetgrentv(NULL, NULL, host, user, domain) == NS_SUCCESS;
}
#endif

#ifdef NSSRC_FILES
int
innetgr(const char *grp, const char *host, const char *user, 
	const char *domain)
{
	int     r, retval;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_innetgr, NULL)
		NS_NIS_CB(_nis_innetgr, NULL)
		NS_NULL_CB
	};

	r = nsdispatch(NULL, dtab, NSDB_NETGROUP, "innetgr",
		       __nsdefaultnis, &retval, grp, host, user, domain);

	return (r == NS_SUCCESS) ? retval : 0;
}
#else
static int
_local_innetgrv(int *rv, void *cbdata, ...)
{
	int e;
	va_list ap;
	va_start(ap, cbdata);
	e = _local_innetgr(rv, cbdata, ap);
	va_end(ap);
	return e;
}

int
innetgr(const char *grp, const char *host, const char *user, 
	const char *domain)
{
	return _local_innetgrv(NULL, NULL, grp, host, user, domain) == NS_SUCCESS;
}
#endif
