/*	$NetBSD: cut.c,v 1.29 2014/02/03 20:22:19 wiz Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting and Marciano Pitargue.
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
static char sccsid[] = "@(#)cut.c	8.3 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: cut.c,v 1.29 2014/02/03 20:22:19 wiz Exp $");
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <wchar.h>
#include <sys/param.h>

static int bflag;
static int	cflag;
static char	dchar;
static int	dflag;
static int	fflag;
static int	sflag;

static void	b_cut(FILE *, const char *);
static void	c_cut(FILE *, const char *);
static void	f_cut(FILE *, const char *);
static void	get_list(char *);
static void	usage(void) __dead;

int
main(int argc, char *argv[])
{
	FILE *fp;
	void (*fcn)(FILE *, const char *);
	int ch, rval;

	fcn = NULL;
	(void)setlocale(LC_ALL, "");

	dchar = '\t';			/* default delimiter is \t */

	/* Since we don't support multi-byte characters, the -c and -b
	   options are equivalent, and the -n option is meaningless. */
	while ((ch = getopt(argc, argv, "b:c:d:f:sn")) != -1)
		switch(ch) {
		case 'b':
			fcn = b_cut;
			get_list(optarg);
			bflag = 1;
			break;
		case 'c':
			fcn = c_cut;
			get_list(optarg);
			cflag = 1;
			break;
		case 'd':
			dchar = *optarg;
			dflag = 1;
			break;
		case 'f':
			get_list(optarg);
			fcn = f_cut;
			fflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'n':
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (fflag) {
		if (cflag || bflag)
			usage();
	} else if ((!cflag && !bflag) || dflag || sflag)
		usage();
	else if (bflag && cflag)
		usage();

	rval = 0;
	if (*argv)
		for (; *argv; ++argv) {
			if (strcmp(*argv, "-") == 0)
				fcn(stdin, "stdin");
			else {
				if ((fp = fopen(*argv, "r"))) {
					fcn(fp, *argv);
					(void)fclose(fp);
				} else {
					rval = 1;
					warn("%s", *argv);
				}
			}
		}
	else
		fcn(stdin, "stdin");
	return(rval);
}

static size_t autostart, autostop, maxval;

static char *positions = NULL;
static size_t numpositions = 0;
#define ALLOC_CHUNK	_POSIX2_LINE_MAX	/* malloc granularity */

static void
get_list(char *list)
{
	size_t setautostart, start, stop;
	char *pos;
	char *p;

	if (positions == NULL) {
		numpositions = ALLOC_CHUNK;
		positions = ecalloc(numpositions, sizeof(*positions));
	}

	/*
	 * set a byte in the positions array to indicate if a field or
	 * column is to be selected; use +1, it's 1-based, not 0-based.
	 * This parser is less restrictive than the Draft 9 POSIX spec.
	 * POSIX doesn't allow lists that aren't in increasing order or
	 * overlapping lists.  We also handle "-3-5" although there's no
	 * real reason to.
	 */
	for (; (p = strtok(list, ", \t")) != NULL; list = NULL) {
		setautostart = start = stop = 0;
		if (*p == '-') {
			++p;
			setautostart = 1;
		}
		if (isdigit((unsigned char)*p)) {
			start = stop = strtol(p, &p, 10);
			if (setautostart && start > autostart)
				autostart = start;
		}
		if (*p == '-') {
			if (isdigit((unsigned char)p[1]))
				stop = strtol(p + 1, &p, 10);
			if (*p == '-') {
				++p;
				if (!autostop || autostop > stop)
					autostop = stop;
			}
		}
		if (*p)
			errx(1, "[-bcf] list: illegal list value");
		if (!stop || !start)
			errx(1, "[-bcf] list: values may not include zero");
		if (stop + 1 > numpositions) {
			size_t newsize;
			newsize = roundup(stop + 1, ALLOC_CHUNK);
			positions = erealloc(positions, newsize);
			(void)memset(positions + numpositions, 0,
			    newsize - numpositions);
			numpositions = newsize;
		}
		if (maxval < stop)
			maxval = stop;
		for (pos = positions + start; start++ <= stop; pos++)
			*pos = 1;
	}

	/* overlapping ranges */
	if (autostop && maxval > autostop)
		maxval = autostop;

	/* set autostart */
	if (autostart)
		(void)memset(positions + 1, '1', autostart);
}

static void
/*ARGSUSED*/
f_cut(FILE *fp, const char *fname __unused)
{
	int ch, field, isdelim;
	char *pos, *p, sep;
	int output;
	size_t len;
	char *lbuf, *tbuf;

	for (sep = dchar, tbuf = NULL; (lbuf = fgetln(fp, &len)) != NULL;) {
		output = 0;
		if (lbuf[len - 1] != '\n') {
			/* no newline at the end of the last line so add one */
			if ((tbuf = (char *)malloc(len + 1)) == NULL)
				err(1, NULL);
			(void)memcpy(tbuf, lbuf, len);
			tbuf[len++] = '\n';
			lbuf = tbuf;
		}
		for (isdelim = 0, p = lbuf;; ++p) {
			ch = *p;
			/* this should work if newline is delimiter */
			if (ch == sep)
				isdelim = 1;
			if (ch == '\n') {
				if (!isdelim && !sflag)
					(void)fwrite(lbuf, len, 1, stdout);
				break;
			}
		}
		if (!isdelim)
			continue;

		pos = positions + 1;
		for (field = maxval, p = lbuf; field; --field, ++pos) {
			if (*pos) {
				if (output++)
					(void)putchar(sep);
				while ((ch = *p++) != '\n' && ch != sep)
					(void)putchar(ch);
			} else {
				while ((ch = *p++) != '\n' && ch != sep)
					continue;
			}
			if (ch == '\n')
				break;
		}
		if (ch != '\n') {
			if (autostop) {
				if (output)
					(void)putchar(sep);
				for (; (ch = *p) != '\n'; ++p)
					(void)putchar(ch);
			} else
				for (; (ch = *p) != '\n'; ++p);
		}
		(void)putchar('\n');
		if (tbuf) {
			free(tbuf);
			tbuf = NULL;
		}
	}
	if (tbuf)
		free(tbuf);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage:\tcut -b list [-n] [file ...]\n"
	    "\tcut -c list [file ...]\n"
	    "\tcut -f list [-d string] [-s] [file ...]\n");
	exit(1);
}

/* make b_put(): */
#define CUT_BYTE 1
#include "x_cut.c"
#undef CUT_BYTE

/* make c_put(): */
#define CUT_BYTE 0
#include "x_cut.c"
#undef CUT_BYTE
