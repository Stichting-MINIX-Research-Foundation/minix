/*	$NetBSD: manconf.c,v 1.6 2008/03/08 15:48:27 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1995
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

/*
 * manconf.c: provides interface for reading man.conf files
 *
 * note that this code is shared across all programs that read man.conf.
 * (currently: apropos, catman, makewhatis, man, and whatis...)
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)config.c	8.8 (Berkeley) 1/31/95";
#else
__RCSID("$NetBSD: manconf.c,v 1.6 2008/03/08 15:48:27 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "manconf.h"
#include "pathnames.h"

TAILQ_HEAD(_head, _tag);
static struct _head head;	/* 'head' -- top level data structure */

/*
 * xstrdup: like strdup, but also returns length of string in lenp
 */
static char *
xstrdup(const char *str, size_t *lenp)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	copy = malloc(len);
	if (!copy)
		return NULL;
	(void)memcpy(copy, str, len);
	if (lenp)
		*lenp = len - 1;	/* subtract out the null */
	return copy;
}

/*
 * config --
 *
 * Read the configuration file and build a doubly linked
 * list off of "head" that looks like:
 *
 *	tag1 <-> entry <-> entry <-> entry
 *	|
 *	tag2 <-> entry <-> entry <-> entry
 *
 * note: will err/errx out on error (fopen or malloc failure)
 */
void
config(const char *fname)
{
	TAG *tp;
	FILE *cfp;
	size_t len;
	int lcnt;
	char *p, *t, type;

	if (fname == NULL)
		fname = _PATH_MANCONF;
	if ((cfp = fopen(fname, "r")) == NULL)
		err(EXIT_FAILURE, "%s", fname);
	TAILQ_INIT(&head);
	for (lcnt = 1; (p = fgetln(cfp, &len)) != NULL; ++lcnt) {
		if (len == 1)			/* Skip empty lines. */
			continue;
		if (p[len - 1] != '\n') {	/* Skip corrupted lines. */
			warnx("%s: line %d corrupted", fname, lcnt);
			continue;
		}
		p[len - 1] = '\0';		/* Terminate the line. */

						/* Skip leading space. */
		for (/*EMPTY*/; *p != '\0' && isspace((unsigned char)*p); ++p)
			continue;
						/* Skip empty/comment lines. */
		if (*p == '\0' || *p == '#')
			continue;
						/* Find first token. */
		for (t = p; *t && !isspace((unsigned char)*t); ++t)
			continue;
		if (*t == '\0')			/* Need more than one token.*/
			continue;
		*t = '\0';

		tp = gettag(p, 1);
		if (!tp)
			errx(EXIT_FAILURE, "gettag: malloc failed");

		/*
		 * Attach new records. Check to see if it is a
		 * section record or not.
		 */

		if (*p == '_') {		/* not a section record */
			/*
			 * Special cases: _build and _crunch take the
			 * rest of the line as a single entry.
			 */
			if (!strcmp(p, "_build") || !strcmp(p, "_crunch")) {
				/*
				 * The reason we're not just using
				 * strtok(3) for all of the parsing is
				 * so we don't get caught if a line
				 * has only a single token on it.
				 */
				while (*++t && isspace((unsigned char)*t));
				if (addentry(tp, t, 0) == -1)
					errx(EXIT_FAILURE,
					    "addentry: malloc failed");
			} else {
				for (++t; (p = strtok(t, " \t\n")) != NULL;
				     t = NULL) {
					if (addentry(tp, p, 0) == -1)
						errx(EXIT_FAILURE,
						   "addentry: malloc failed");
				}
			}

		} else {			/* section record */

			/*
			 * section entries can either be all absolute
			 * paths or all relative paths, but not both.
			 */
			type = (TAILQ_FIRST(&tp->entrylist) != NULL) ?
			    *(TAILQ_FIRST(&tp->entrylist)->s) : 0;

			for (++t; (p = strtok(t, " \t\n")) != NULL; t = NULL) {

				/* ensure an assigned type */
				if (type == 0)
					type = *p;

				/* check for illegal mix */
				if (*p != type) {
	warnx("section %s: %s: invalid entry, does not match previous types",
	      tp->s, p);
	warnx("man.conf cannot mix absolute and relative paths in an entry");
					continue;
				}
				if (addentry(tp, p, 0) == -1)
					errx(EXIT_FAILURE,
					    "addentry: malloc failed");
			}
		}
	}
	(void)fclose(cfp);
}

/*
 * gettag --
 *	if (!create) return tag for given name if it exists, or NULL otherwise
 *
 *	if (create) return tag for given name if it exists, try and create
 *	a new tag if it does not exist.  return NULL if unable to create new
 *	tag.
 */
TAG *
gettag(const char *name, int create)
{
	TAG *tp;

	TAILQ_FOREACH(tp, &head, q)
		if (!strcmp(name, tp->s))
			return tp;
	if (!create)
		return NULL;

	/* try and add it in */
	tp = malloc(sizeof(*tp));
	if (tp)
		tp->s = xstrdup(name, &tp->len);
	if (!tp || !tp->s) {
		if (tp)
			free(tp);
		return NULL;
	}
	TAILQ_INIT(&tp->entrylist);
	TAILQ_INSERT_TAIL(&head, tp, q);
	return tp;
}

/*
 * addentry --
 *	add an entry to a list.
 *	returns -1 if malloc failed, otherwise 0.
 */
int
addentry(TAG *tp, const char *newent, int ishead)
{
	ENTRY *ep;

	ep = malloc(sizeof(*ep));
	if (ep)
		ep->s = xstrdup(newent, &ep->len);
	if (!ep || !ep->s) {
		if (ep)
			free(ep);
		return -1;
	}
	if (ishead)
		TAILQ_INSERT_HEAD(&tp->entrylist, ep, q);
	else
		TAILQ_INSERT_TAIL(&tp->entrylist, ep, q);

	return 0;
}
