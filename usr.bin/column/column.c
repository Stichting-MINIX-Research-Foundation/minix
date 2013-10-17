/*	$NetBSD: column.c,v 1.21 2008/07/21 14:19:21 lukem Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)column.c	8.4 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: column.c,v 1.21 2008/07/21 14:19:21 lukem Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <termios.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define	TAB	8
#define TABROUND(l) 	(((l) + TAB) & ~(TAB - 1))

static void  c_columnate(void);
static void  input(FILE *);
static void  maketbl(void);
static void  print(void);
static void  r_columnate(void);
static void  usage(void) __dead;

static int termwidth = 80;		/* default terminal width */

static int entries;			/* number of records */
static int eval;			/* exit value */
static int maxlength;			/* longest record */
static char **list;			/* array of pointers to records */
static const char *separator = "\t ";	/* field separator for table option */

int
main(int argc, char **argv)
{
	struct winsize win;
	FILE *fp;
	int ch, tflag, xflag;
	const char *p;

	setprogname(*argv);

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == -1 || !win.ws_col) {
		if ((p = getenv("COLUMNS")) != NULL)
			termwidth = atoi(p);
	} else
		termwidth = win.ws_col;

	tflag = xflag = 0;
	while ((ch = getopt(argc, argv, "c:s:tx")) != -1)
		switch(ch) {
		case 'c':
			termwidth = atoi(optarg);
			break;
		case 's':
			separator = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!*argv)
		input(stdin);
	else for (; *argv; ++argv)
		if ((fp = fopen(*argv, "r")) != NULL) {
			input(fp);
			(void)fclose(fp);
		} else {
			warn("Cannot open `%s'", *argv);
			eval = 1;
		}

	if (!entries)
		return eval;

	maxlength = TABROUND(maxlength);
	if (tflag)
		maketbl();
	else if (maxlength >= termwidth)
		print();
	else if (xflag)
		c_columnate();
	else
		r_columnate();
	return eval;
}

static void
c_columnate(void)
{
	int chcnt, col, cnt, endcol, numcols;
	char **lp;

	numcols = termwidth / maxlength;
	endcol = maxlength;
	for (chcnt = col = 0, lp = list;; ++lp) {
		chcnt += printf("%s", *lp);
		if (!--entries)
			break;
		if (++col == numcols) {
			chcnt = col = 0;
			endcol = maxlength;
			(void)putchar('\n');
		} else {
			while ((cnt = TABROUND(chcnt)) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += maxlength;
		}
	}
	if (chcnt)
		(void)putchar('\n');
}

static void
r_columnate(void)
{
	int base, chcnt, cnt, col, endcol, numcols, numrows, row;

	numcols = termwidth / maxlength;
	numrows = entries / numcols;
	if (entries % numcols)
		++numrows;

	for (row = 0; row < numrows; ++row) {
		endcol = maxlength;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += printf("%s", list[base]);
			if ((base += numrows) >= entries)
				break;
			while ((cnt = TABROUND(chcnt)) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += maxlength;
		}
		(void)putchar('\n');
	}
}

static void
print(void)
{
	int cnt;
	char **lp;

	for (cnt = entries, lp = list; cnt--; ++lp)
		(void)printf("%s\n", *lp);
}

typedef struct _tbl {
	char **list;
	int cols, *len;
} TBL;
#define	DEFCOLS	25

static void
maketbl(void)
{
	TBL *t;
	int coloff, cnt;
	char *p, **lp;
	int *lens, *nlens, maxcols;
	TBL *tbl;
	char **cols, **ncols;

	t = tbl = ecalloc(entries, sizeof(*t));
	cols = ecalloc((maxcols = DEFCOLS), sizeof(*cols));
	lens = ecalloc(maxcols, sizeof(*lens));
	for (cnt = 0, lp = list; cnt < entries; ++cnt, ++lp, ++t) {
		for (coloff = 0, p = *lp;
		    (cols[coloff] = strtok(p, separator)) != NULL; p = NULL)
			if (++coloff == maxcols) {
				ncols = erealloc(cols, (maxcols +
				    DEFCOLS) * sizeof(*ncols));
				nlens = erealloc(lens, (maxcols +
				    DEFCOLS) * sizeof(*nlens));
				cols = ncols;
				lens = nlens;
				(void)memset(cols + maxcols, 0,
				    DEFCOLS * sizeof(*cols));
				(void)memset(lens + maxcols, 0,
				    DEFCOLS * sizeof(*lens));
				maxcols += DEFCOLS;
			}
		t->list = ecalloc(coloff, sizeof(*(t->list)));
		t->len = ecalloc(coloff, sizeof(*(t->len)));
		for (t->cols = coloff; --coloff >= 0;) {
			t->list[coloff] = cols[coloff];
			t->len[coloff] = strlen(cols[coloff]);
			if (t->len[coloff] > lens[coloff])
				lens[coloff] = t->len[coloff];
		}
	}
	for (cnt = 0, t = tbl; cnt < entries; ++cnt, ++t) {
		for (coloff = 0; coloff < t->cols  - 1; ++coloff)
			(void)printf("%s%*s", t->list[coloff],
			    lens[coloff] - t->len[coloff] + 2, " ");
		(void)printf("%s\n", t->list[coloff]);
	}
	free(tbl);
	free(cols);
	free(lens);
}

#define	DEFNUM		1000

static void
input(FILE *fp)
{
	static int maxentry;
	int len;
	size_t blen;
	char *p, *buf;
	char **n;

	if (!list)
		list = ecalloc((maxentry = DEFNUM), sizeof(*list));
	while ((buf = fgetln(fp, &blen)) != NULL) {
		buf = estrndup(buf, blen);
		for (p = buf; *p && isspace((unsigned char)*p); ++p);
		if (!*p) {
			free(buf);
			continue;
		}
		if (!(p = strchr(p, '\n'))) {
			warnx("line too long");
			eval = 1;
			free(buf);
			continue;
		}
		*p = '\0';
		len = p - buf;
		if (maxlength < len)
			maxlength = len;
		if (entries == maxentry) {
			n = erealloc(list, (maxentry + DEFNUM) * sizeof(*n));
			(void)memset(n + maxentry, 0, sizeof(*n) * DEFNUM);
			maxentry += DEFNUM;
			list = n;
		}
		list[entries++] = buf;
	}
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "Usage: %s [-tx] [-c columns] [-s sep] [file ...]\n",
	    getprogname());
	exit(1);
}
