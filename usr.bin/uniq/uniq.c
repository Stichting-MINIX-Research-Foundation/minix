/*	$NetBSD: uniq.c,v 1.18 2012/08/26 14:14:16 wiz Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)uniq.c	8.3 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: uniq.c,v 1.18 2012/08/26 14:14:16 wiz Exp $");
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int cflag, dflag, uflag;
static int numchars, numfields, repeats;

static FILE *file(const char *, const char *);
static void show(FILE *, const char *);
static const char *skip(const char *);
static void obsolete(char *[]);
static void usage(void) __dead;

int
main (int argc, char *argv[])
{
	const char *t1, *t2;
	FILE *ifp, *ofp;
	int ch;
	char *prevline, *thisline, *p;
	size_t prevlinesize, thislinesize, psize;

	setprogname(argv[0]);
	ifp = ofp = NULL;
	obsolete(argv);
	while ((ch = getopt(argc, argv, "-cdf:s:u")) != -1)
		switch (ch) {
		case '-':
			--optind;
			goto done;
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			numfields = strtol(optarg, &p, 10);
			if (numfields < 0 || *p)
				errx(1, "illegal field skip value: %s", optarg);
			break;
		case 's':
			numchars = strtol(optarg, &p, 10);
			if (numchars < 0 || *p)
				errx(1, "illegal character skip value: %s",
				    optarg);
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
	}

done:	argc -= optind;
	argv +=optind;

	switch(argc) {
	case 0:
		ifp = stdin;
		ofp = stdout;
		break;
	case 1:
		ifp = file(argv[0], "r");
		ofp = stdout;
		break;
	case 2:
		ifp = file(argv[0], "r");
		ofp = file(argv[1], "w");
		break;
	default:
		usage();
	}

	if ((p = fgetln(ifp, &psize)) == NULL)
		return 0;
	prevlinesize = psize;
	if ((prevline = malloc(prevlinesize + 1)) == NULL)
		err(1, "malloc");
	(void)memcpy(prevline, p, prevlinesize);
	prevline[prevlinesize] = '\0';

	thislinesize = psize;
	if ((thisline = malloc(thislinesize + 1)) == NULL)
		err(1, "malloc");

	while ((p = fgetln(ifp, &psize)) != NULL) {
		if (psize > thislinesize) {
			if ((thisline = realloc(thisline, psize + 1)) == NULL)
				err(1, "realloc");
			thislinesize = psize;
		}
		(void)memcpy(thisline, p, psize);
		thisline[psize] = '\0';

		/* If requested get the chosen fields + character offsets. */
		if (numfields || numchars) {
			t1 = skip(thisline);
			t2 = skip(prevline);
		} else {
			t1 = thisline;
			t2 = prevline;
		}

		/* If different, print; set previous to new value. */
		if (strcmp(t1, t2)) {
			char *t;
			size_t ts;

			show(ofp, prevline);
			t = prevline;
			prevline = thisline;
			thisline = t;
			ts = prevlinesize;
			prevlinesize = thislinesize;
			thislinesize = ts;
			repeats = 0;
		} else
			++repeats;
	}
	show(ofp, prevline);
	free(prevline);
	free(thisline);
	return 0;
}

/*
 * show --
 *	Output a line depending on the flags and number of repetitions
 *	of the line.
 */
static void
show(FILE *ofp, const char *str)
{

	if ((dflag && repeats == 0) || (uflag && repeats > 0))
		return;
	if (cflag) {
		(void)fprintf(ofp, "%4d %s", repeats + 1, str);
	} else {
		(void)fprintf(ofp, "%s", str);
	}
}

static const char *
skip(const char *str)
{
	int infield, nchars, nfields;

	for (nfields = numfields, infield = 0; nfields && *str; ++str)
		if (isspace((unsigned char)*str)) {
			if (infield) {
				infield = 0;
				--nfields;
			}
		} else if (!infield)
			infield = 1;
	for (nchars = numchars; nchars-- && *str; ++str)
		continue;
	return str;
}

static FILE *
file(const char *name, const char *mode)
{
	FILE *fp;

	if ((fp = fopen(name, mode)) == NULL)
		err(1, "%s", name);
	return(fp);
}

static void
obsolete(char *argv[])
{
	char *ap, *p, *start;

	while ((ap = *++argv) != NULL) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-')
			return;
		if (!isdigit((unsigned char)ap[1]))
			continue;
		/*
		 * Digit signifies an old-style option.  Malloc space for dash,
		 * new option and argument.
		 */
		(void)asprintf(&p, "-%c%s", ap[0] == '+' ? 's' : 'f', ap + 1);
		if (!p)
			err(1, "malloc");
		start = p;
		*argv = start;
	}
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-cdu] [-f fields] [-s chars] "
	    "[input_file [output_file]]\n", getprogname());
	exit(1);
}
