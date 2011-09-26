/*	$NetBSD: apropos.c,v 1.30 2009/05/08 12:48:43 wiz Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)apropos.c	8.8 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: apropos.c,v 1.30 2009/05/08 12:48:43 wiz Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <glob.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "manconf.h"		/* from ../man/ */
#include "pathnames.h"		/* from ../man/ */

static bool *found;
static bool foundman = false;

#define	MAXLINELEN	8192		/* max line handled */

static void apropos(char **, const char *, bool, const char *, const char *);
static void lowstr(const char *, char *);
static bool match(const char *, const char *);
static void usage(void) __dead;

int
main(int argc, char *argv[])
{
	ENTRY *ep;
	TAG *tp;
	int ch, rv;
	char *conffile, *machine, **p, *p_augment, *p_path, *sflag;
	glob_t pg;

	conffile = NULL;
	p_augment = p_path = NULL;
	machine = sflag = NULL;
	while ((ch = getopt(argc, argv, "C:M:m:P:S:s:")) != -1) {
		switch (ch) {
		case 'C':
			conffile = optarg;
			break;
		case 'M':
		case 'P':		/* backward compatible */
			p_path = optarg;
			break;
		case 'm':
			p_augment = optarg;
			break;
		case 'S':
			machine = optarg;
			lowstr(machine, machine);
			break;
		case 's':
			sflag = optarg;
			lowstr(sflag, sflag);
			break;
		case '?':
		default:
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	if (argc < 1)
		usage();

	if ((found = malloc(argc * sizeof(*found))) == NULL)
		err(EXIT_FAILURE, "malloc");
	(void)memset(found, 0, argc * sizeof(*found));

	for (p = argv; *p; ++p)			/* convert to lower-case */
		lowstr(*p, *p);

	if (p_augment)
		apropos(argv, p_augment, true, sflag, machine);
	if (p_path || (p_path = getenv("MANPATH")))
		apropos(argv, p_path, true, sflag, machine);
	else {
		config(conffile);
		tp = gettag("_whatdb", 1);
		if (!tp)
			errx(EXIT_FAILURE, "malloc");
		TAILQ_FOREACH(ep, &tp->entrylist, q) {
			if ((rv = glob(ep->s, GLOB_BRACE | GLOB_NOSORT, NULL,
			    &pg)) != 0) {
				if (rv == GLOB_NOMATCH)
					continue;
				else
					err(EXIT_FAILURE, "glob");
			}
			if (pg.gl_pathc)
				for (p = pg.gl_pathv; *p; p++)
					apropos(argv, *p, false, sflag,
						machine);
			globfree(&pg);
		}
	}

	if (!foundman)
		errx(EXIT_FAILURE, "no %s file found", _PATH_WHATIS);

	rv = 1;
	for (p = argv; *p; ++p)
		if (found[p - argv])
			rv = 0;
		else
			(void)printf("%s: nothing appropriate\n", *p);
	return rv;
}

static void
apropos(char **argv, const char *path, bool buildpath,
	const char *sflag, const char *machine)
{
	char *end, **p;
	const char *name;
	char buf[MAXLINELEN + 1];
	char hold[MAXPATHLEN + 1];
	char wbuf[MAXLINELEN + 1];
	size_t slen = 0, mlen = 0;

	if (sflag)
		slen = strlen(sflag);
	if (machine)
		mlen = strlen(machine);

	for (name = path; name; name = end) {	/* through name list */
		if ((end = strchr(name, ':')) != NULL)
			*end++ = '\0';

		if (buildpath) {
			(void)snprintf(hold, sizeof(hold), "%s/%s", name,
					_PATH_WHATIS);
			name = hold;
		}

		if (!freopen(name, "r", stdin))
			continue;

		foundman = true;

		/* for each file found */
		while (fgets(buf, (int)sizeof(buf), stdin)) {
			if (!strchr(buf, '\n')) {
				warnx("%s: line too long", name);
				continue;
			}
			lowstr(buf, wbuf);
			if (machine) {
				if ((strncmp(wbuf, machine, mlen) != 0) ||
				    strlen(wbuf) <= mlen || wbuf[mlen] != '/')
					continue;
			}
			if (sflag) {
				char *s = strchr(wbuf, '(');

				if (!s)
					continue;
				if (strncmp(s+1, sflag, slen) != 0)
					continue;
			}
			for (p = argv; *p; ++p) {
				if (match(wbuf, *p)) {
					(void)printf("%s", buf);
					found[p - argv] = true;

					/* only print line once */
					while (*++p)
						if (match(wbuf, *p))
							found[p - argv] = true;
					break;
				}
			}
		}
	}
}

/*
 * match --
 *	match anywhere the string appears
 */
static bool
match(const char *bp, const char *str)
{
	size_t len;
	char test;

	if (!*bp)
		return false;
	/* backward compatible: everything matches empty string */
	if (!*str)
		return true;
	for (test = *str++, len = strlen(str); *bp;)
		if (test == *bp++ && !strncmp(bp, str, len))
			return true;
	return false;
}

/*
 * lowstr --
 *	convert a string to lower case
 */
static void
lowstr(const char *from, char *to)
{
	char ch;

	while ((ch = *from++) && ch != '\n')
		*to++ = tolower((unsigned char)ch);
	*to = '\0';
}

/*
 * usage --
 *	print usage message and die
 */
__dead
static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-C file] [-M path] [-m path] "
	    "[-S subsection] [-s section]\n"
	    "       keyword ...\n",
	    getprogname());
	exit(1);
}
