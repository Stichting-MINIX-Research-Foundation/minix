/*	$NetBSD: services_mkdb.c,v 1.18 2010/10/07 01:28:50 christos Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn and Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: services_mkdb.c,v 1.18 2010/10/07 01:28:50 christos Exp $");
#endif /* not lint */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <ctype.h>
#include <errno.h>
#include <stringlist.h>

#include "extern.h"

static char tname[MAXPATHLEN];

#define	PMASK		0xffff
#define PROTOMAX	6

static StringList ***parseservices(const char *, StringList *);
static void	cleanup(void);
static char    *getstring(const char *, size_t, char **, const char *);
static size_t	getprotoindex(StringList *, const char *);
static const char *getprotostr(StringList *, size_t);
static void	usage(void) __dead;

int
main(int argc, char *argv[])
{
	int	 ch;
	const char *fname = _PATH_SERVICES;
	const char *dbname = NULL;
	int	 use_db = 0;
	int	 warndup = 0;
	int	 unique = 0;
	int	 otherflag = 0;
	size_t	 cnt = 0;
	StringList *sl, ***svc;
	size_t port, proto;
	void (*addfn)(StringList *, size_t, const char *, size_t *, int);
	int (*closefn)(void);

	setprogname(argv[0]);

	while ((ch = getopt(argc, argv, "o:quV:v")) != -1)
		switch (ch) {
		case 'o':
			otherflag = 1;
			dbname = optarg;
			break;
		case 'q':
			otherflag = 1;
			warndup = 0;
			break;
		case 'u':
			unique++;
			break;
		case 'V':
			if (strcmp(optarg, "db") == 0)
				use_db = 1;
			else if (strcmp(optarg, "cdb") == 0)
				use_db = 0;
			else
				usage();
			break;
		case 'v':
			otherflag = 1;
			warndup = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 1 || (unique && otherflag))
		usage();
	if (argc == 1)
		fname = argv[0];

	if (unique)
		uniq(fname);

	if (dbname == NULL)
		dbname = use_db ? _PATH_SERVICES_DB : _PATH_SERVICES_CDB;

	svc = parseservices(fname, sl = sl_init());

	if (atexit(cleanup))
		err(1, "Cannot install exit handler");

	(void)snprintf(tname, sizeof(tname), "%s.tmp", dbname);

	if (use_db) {
		if (db_open(tname))
			err(1, "Error opening temporary database `%s'", tname);
		addfn = db_add;
		closefn = db_close;
	} else {
		if (cdb_open(tname))
			err(1, "Error opening temporary database `%s'", tname);
		addfn = cdb_add;
		closefn = cdb_close;
	}

	for (port = 0; port < PMASK + 1; port++) {
		if (svc[port] == NULL)
			continue;

		for (proto = 0; proto < PROTOMAX; proto++) {
			StringList *s;
			if ((s = svc[port][proto]) == NULL)
				continue;
			(addfn)(s, port, getprotostr(sl, proto), &cnt, warndup);
		}

		free(svc[port]);
	}

	free(svc);
	sl_free(sl, 1);

	if ((closefn)())
		err(1, "Error writing temporary database `%s'", tname);

	if (rename(tname, dbname) == -1)
		err(1, "Cannot rename `%s' to `%s'", tname, dbname);

	return 0;
}

static StringList ***
parseservices(const char *fname, StringList *sl)
{
	size_t len, line, pindex;
	FILE *fp;
	StringList ***svc, *s;
	char *p, *ep;

	if ((fp = fopen(fname, "r")) == NULL)
		err(1, "Cannot open `%s'", fname);

	line = 0;
	svc = ecalloc(PMASK + 1, sizeof(StringList **));

	/* XXX: change NULL to "\0\0#" when fparseln fixed */
	for (; (p = fparseln(fp, &len, &line, NULL, 0)) != NULL; free(p)) {
		char	*name, *port, *proto, *aliases, *cp, *alias;
		unsigned long pnum;

		if (len == 0)
			continue;

		for (cp = p; *cp && isspace((unsigned char)*cp); cp++)
			continue;

		if (*cp == '\0' || *cp == '#')
			continue;

		if ((name = getstring(fname, line, &cp, "name")) == NULL)
			continue;

		if ((port = getstring(fname, line, &cp, "port")) == NULL)
			continue;

		if (cp) {
			for (aliases = cp; *cp && *cp != '#'; cp++)
				continue;

			if (*cp)
				*cp = '\0';
		} else
			aliases = NULL;

		proto = strchr(port, '/');
		if (proto == NULL || proto[1] == '\0') {
			warnx("%s, %zu: no protocol found", fname, line);
			continue;
		}
		*proto++ = '\0';

		errno = 0;
		pnum = strtoul(port, &ep, 0);
		if (*port == '\0' || *ep != '\0') {
			warnx("%s, %zu: invalid port `%s'", fname, line, port);
			continue;
		}
		if ((errno == ERANGE && pnum == ULONG_MAX) || pnum > PMASK) {
			warnx("%s, %zu: port too big `%s'", fname, line, port);
			continue;
		}

		if (svc[pnum] == NULL)
			svc[pnum] = ecalloc(PROTOMAX, sizeof(StringList *));

		pindex = getprotoindex(sl, proto);
		if (svc[pnum][pindex] == NULL)
			s = svc[pnum][pindex] = sl_init();
		else
			s = svc[pnum][pindex];
		
		if (strlen(name) > 255) {
			warnx("%s, %zu: invalid name too long `%s'", fname,
			    line, name);
			continue;
		}
		
		/* build list of aliases */
		if (sl_find(s, name) == NULL)
			(void)sl_add(s, estrdup(name));

		if (aliases) {
			while ((alias = strsep(&aliases, " \t")) != NULL) {
				if (alias[0] == '\0')
					continue;
				if (strlen(alias) > 255) {
					warnx("%s, %zu: alias name too long `%s'",
					    fname, line, alias);
		    			continue;
				}
				if (sl_find(s, alias) == NULL)
					(void)sl_add(s, estrdup(alias));
			}
		}
	}
	(void)fclose(fp);
	return svc;
}

/*
 * cleanup(): Remove temporary files upon exit
 */
static void
cleanup(void)
{
	if (tname[0])
		(void)unlink(tname);
}

static char *
getstring(const char *fname, size_t line, char **cp, const char *tag)
{
	char *str;

	while ((str = strsep(cp, " \t")) != NULL && *str == '\0')
		continue;

	if (str == NULL)
		warnx("%s, %zu: no %s found", fname, line, tag);

	return str;
}

static size_t
getprotoindex(StringList *sl, const char *str)
{
	size_t i;

	for (i= 0; i < sl->sl_cur; i++)
		if (strcmp(sl->sl_str[i], str) == 0)
			return i;

	if (i == PROTOMAX)
		errx(1, "Ran out of protocols adding `%s';"
		    " recompile with larger PROTOMAX", str);
	(void)sl_add(sl, estrdup(str));
	return i;
}

static const char *
getprotostr(StringList *sl, size_t i)
{
	assert(i < sl->sl_cur);
	return sl->sl_str[i];
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage:\t%s [-q] [-o <db>] [-V cdb|db] [<servicefile>]\n"
	    "\t%s -u [<servicefile>]\n", getprogname(), getprogname());
	exit(1);
}
