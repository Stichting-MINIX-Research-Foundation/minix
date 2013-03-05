/*	$NetBSD: col.c,v 1.17 2011/08/31 16:24:57 plunky Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Rendell of the Memorial University of Newfoundland.
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
__COPYRIGHT("@(#) Copyright (c) 1990, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)col.c	8.5 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: col.c,v 1.17 2011/08/31 16:24:57 plunky Exp $");
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	BS	'\b'		/* backspace */
#define	TAB	'\t'		/* tab */
#define	SPACE	' '		/* space */
#define	NL	'\n'		/* newline */
#define	CR	'\r'		/* carriage return */
#define	ESC	'\033'		/* escape */
#define	SI	'\017'		/* shift in to normal character set */
#define	SO	'\016'		/* shift out to alternate character set */
#define	VT	'\013'		/* vertical tab (aka reverse line feed) */
#define	RLF	'\007'		/* ESC-07 reverse line feed */
#define	RHLF	'\010'		/* ESC-010 reverse half-line feed */
#define	FHLF	'\011'		/* ESC-011 forward half-line feed */

/* build up at least this many lines before flushing them out */
#define	BUFFER_MARGIN		32

typedef char CSET;

typedef struct char_str {
#define	CS_NORMAL	1
#define	CS_ALTERNATE	2
	short		c_column;	/* column character is in */
	CSET		c_set;		/* character set (currently only 2) */
	char		c_char;		/* character in question */
} CHAR;

typedef struct line_str LINE;
struct line_str {
	CHAR	*l_line;		/* characters on the line */
	LINE	*l_prev;		/* previous line */
	LINE	*l_next;		/* next line */
	int	l_lsize;		/* allocated sizeof l_line */
	int	l_line_len;		/* strlen(l_line) */
	int	l_needs_sort;		/* set if chars went in out of order */
	int	l_max_col;		/* max column in the line */
};

static LINE   *alloc_line(void);
static void	dowarn(int);
static void	flush_line(LINE *);
static void	flush_lines(int);
static void	flush_blanks(void);
static void	free_line(LINE *);
__dead static void	usage(void);
__dead static void	wrerr(void);
static void   *xmalloc(void *, size_t);

static CSET	last_set;		/* char_set of last char printed */
static LINE   *lines;
static int	compress_spaces;	/* if doing space -> tab conversion */
static int	fine;			/* if `fine' resolution (half lines) */
static int	max_bufd_lines;		/* max # lines to keep in memory */
static int	nblank_lines;		/* # blanks after last flushed line */
static int	no_backspaces;		/* if not to output any backspaces */
static int	pass_unknown_seqs;	/* whether to pass unknown control sequences */

#define	PUTC(ch) \
	if (putchar(ch) == EOF) \
		wrerr();

int
main(int argc, char **argv)
{
	int ch;
	CHAR *c;
	CSET cur_set;			/* current character set */
	LINE *l;			/* current line */
	int extra_lines;		/* # of lines above first line */
	int cur_col;			/* current column */
	int cur_line;			/* line number of current position */
	int max_line;			/* max value of cur_line */
	int this_line;			/* line l points to */
	int nflushd_lines;		/* number of lines that were flushed */
	int adjust, opt, warned;

	max_bufd_lines = 128;
	compress_spaces = 1;		/* compress spaces into tabs */
	pass_unknown_seqs = 0;		/* remove unknown escape sequences */
	while ((opt = getopt(argc, argv, "bfhl:px")) != -1)
		switch (opt) {
		case 'b':		/* do not output backspaces */
			no_backspaces = 1;
			break;
		case 'f':		/* allow half forward line feeds */
			fine = 1;
			break;
		case 'h':		/* compress spaces into tabs */
			compress_spaces = 1;
			break;
		case 'l':		/* buffered line count */
			if ((max_bufd_lines = atoi(optarg)) <= 0) {
				(void)fprintf(stderr,
				    "col: bad -l argument %s.\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':		/* pass unknown control sequences */
			pass_unknown_seqs = 1;
			break;
		case 'x':		/* do not compress spaces into tabs */
			compress_spaces = 0;
			break;
		case '?':
		default:
			usage();
		}

	if (optind != argc)
		usage();

	/* this value is in half lines */
	max_bufd_lines *= 2;

	adjust = cur_col = extra_lines = warned = 0;
	cur_line = max_line = nflushd_lines = this_line = 0;
	cur_set = last_set = CS_NORMAL;
	lines = l = alloc_line();

	while ((ch = getchar()) != EOF) {
		if (!isgraph(ch)) {
			switch (ch) {
			case BS:		/* can't go back further */
				if (cur_col == 0)
					continue;
				--cur_col;
				continue;
			case CR:
				cur_col = 0;
				continue;
			case ESC:		/* just ignore EOF */
				switch(getchar()) {
				case RLF:
					cur_line -= 2;
					break;
				case RHLF:
					cur_line--;
					break;
				case FHLF:
					cur_line++;
					if (cur_line > max_line)
						max_line = cur_line;
				}
				continue;
			case NL:
				cur_line += 2;
				if (cur_line > max_line)
					max_line = cur_line;
				cur_col = 0;
				continue;
			case SPACE:
				++cur_col;
				continue;
			case SI:
				cur_set = CS_NORMAL;
				continue;
			case SO:
				cur_set = CS_ALTERNATE;
				continue;
			case TAB:		/* adjust column */
				cur_col |= 7;
				++cur_col;
				continue;
			case VT:
				cur_line -= 2;
				continue;
			}
			if (!pass_unknown_seqs)
				continue;
		}

		/* Must stuff ch in a line - are we at the right one? */
		if (cur_line != this_line - adjust) {
			LINE *lnew;
			int nmove;

			adjust = 0;
			nmove = cur_line - this_line;
			if (!fine) {
				/* round up to next line */
				if (cur_line & 1) {
					adjust = 1;
					nmove++;
				}
			}
			if (nmove < 0) {
				for (; nmove < 0 && l->l_prev; nmove++)
					l = l->l_prev;
				if (nmove) {
					if (nflushd_lines == 0) {
						/*
						 * Allow backup past first
						 * line if nothing has been
						 * flushed yet.
						 */
						for (; nmove < 0; nmove++) {
							lnew = alloc_line();
							l->l_prev = lnew;
							lnew->l_next = l;
							l = lines = lnew;
							extra_lines++;
						}
					} else {
						if (!warned++)
							dowarn(cur_line);
						cur_line -= nmove;
					}
				}
			} else {
				/* may need to allocate here */
				for (; nmove > 0 && l->l_next; nmove--)
					l = l->l_next;
				for (; nmove > 0; nmove--) {
					lnew = alloc_line();
					lnew->l_prev = l;
					l->l_next = lnew;
					l = lnew;
				}
			}
			this_line = cur_line + adjust;
			nmove = this_line - nflushd_lines;
			if (nmove >= max_bufd_lines + BUFFER_MARGIN) {
				nflushd_lines += nmove - max_bufd_lines;
				flush_lines(nmove - max_bufd_lines);
			}
		}
		/* grow line's buffer? */
		if (l->l_line_len + 1 >= l->l_lsize) {
			int need;

			need = l->l_lsize ? l->l_lsize * 2 : 90;
			l->l_line = (CHAR *)xmalloc((void *) l->l_line,
			    (unsigned) need * sizeof(CHAR));
			l->l_lsize = need;
		}
		c = &l->l_line[l->l_line_len++];
		c->c_char = ch;
		c->c_set = cur_set;
		c->c_column = cur_col;
		/*
		 * If things are put in out of order, they will need sorting
		 * when it is flushed.
		 */
		if (cur_col < l->l_max_col)
			l->l_needs_sort = 1;
		else
			l->l_max_col = cur_col;
		cur_col++;
	}
	if (max_line == 0)
		exit(EXIT_SUCCESS);	/* no lines, so just exit */

	/* goto the last line that had a character on it */
	for (; l->l_next; l = l->l_next)
		this_line++;
	flush_lines(this_line - nflushd_lines + extra_lines + 1);

	/* make sure we leave things in a sane state */
	if (last_set != CS_NORMAL)
		PUTC('\017');

	/* flush out the last few blank lines */
	nblank_lines = max_line - this_line;
	if (max_line & 1)
		nblank_lines++;
	else if (!nblank_lines)
		/* missing a \n on the last line? */
		nblank_lines = 2;
	flush_blanks();
	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

static void
flush_lines(int nflush)
{
	LINE *l;

	while (--nflush >= 0) {
		l = lines;
		lines = l->l_next;
		if (l->l_line) {
			flush_blanks();
			flush_line(l);
		}
		nblank_lines++;
		if (l->l_line)
			(void)free((void *)l->l_line);
		free_line(l);
	}
	if (lines)
		lines->l_prev = NULL;
}

/*
 * Print a number of newline/half newlines.  If fine flag is set, nblank_lines
 * is the number of half line feeds, otherwise it is the number of whole line
 * feeds.
 */
static void
flush_blanks(void)
{
	int half, i, nb;

	half = 0;
	nb = nblank_lines;
	if (nb & 1) {
		if (fine)
			half = 1;
		else
			nb++;
	}
	nb /= 2;
	for (i = nb; --i >= 0;)
		PUTC('\n');
	if (half) {
		PUTC('\033');
		PUTC('9');
		if (!nb)
			PUTC('\r');
	}
	nblank_lines = 0;
}

/*
 * Write a line to stdout taking care of space to tab conversion (-h flag)
 * and character set shifts.
 */
static void
flush_line(LINE *l)
{
	CHAR *c, *endc;
	int nchars, last_col, this_col;

	last_col = 0;
	nchars = l->l_line_len;

	if (l->l_needs_sort) {
		static CHAR *sorted;
		static int count_size, *count, i, save, sorted_size, tot;

		/*
		 * Do an O(n) sort on l->l_line by column being careful to
		 * preserve the order of characters in the same column.
		 */
		if (l->l_lsize > sorted_size) {
			sorted_size = l->l_lsize;
			sorted = (CHAR *)xmalloc((void *)sorted,
			    (unsigned)sizeof(CHAR) * sorted_size);
		}
		if (l->l_max_col >= count_size) {
			count_size = l->l_max_col + 1;
			count = (int *)xmalloc((void *)count,
			    (unsigned)sizeof(int) * count_size);
		}
		(void)memset(count, 0, sizeof(int) * l->l_max_col + 1);
		for (i = nchars, c = l->l_line; --i >= 0; c++)
			count[c->c_column]++;

		/*
		 * calculate running total (shifted down by 1) to use as
		 * indices into new line.
		 */
		for (tot = 0, i = 0; i <= l->l_max_col; i++) {
			save = count[i];
			count[i] = tot;
			tot += save;
		}

		for (i = nchars, c = l->l_line; --i >= 0; c++)
			sorted[count[c->c_column]++] = *c;
		c = sorted;
	} else
		c = l->l_line;
	while (nchars > 0) {
		this_col = c->c_column;
		endc = c;
		do {
			++endc;
		} while (--nchars > 0 && this_col == endc->c_column);

		/* if -b only print last character */
		if (no_backspaces)
			c = endc - 1;

		if (this_col > last_col) {
			int nspace = this_col - last_col;

			if (compress_spaces && nspace > 1) {
				int ntabs;

				ntabs = ((last_col % 8) + nspace) / 8;
				if (ntabs) {
					nspace -= (ntabs * 8) - (last_col % 8);
					while (--ntabs >= 0)
						PUTC('\t');
				}
			}
			while (--nspace >= 0)
				PUTC(' ');
			last_col = this_col;
		}
		last_col++;

		for (;;) {
			if (c->c_set != last_set) {
				switch (c->c_set) {
				case CS_NORMAL:
					PUTC('\017');
					break;
				case CS_ALTERNATE:
					PUTC('\016');
				}
				last_set = c->c_set;
			}
			PUTC(c->c_char);
			if (++c >= endc)
				break;
			PUTC('\b');
		}
	}
}

#define	NALLOC 64

static LINE *line_freelist;

static LINE *
alloc_line(void)
{
	LINE *l;
	int i;

	if (!line_freelist) {
		l = (LINE *)xmalloc(NULL, sizeof(LINE) * NALLOC);
		line_freelist = l;
		for (i = 1; i < NALLOC; i++, l++)
			l->l_next = l + 1;
		l->l_next = NULL;
	}
	l = line_freelist;
	line_freelist = l->l_next;

	(void)memset(l, 0, sizeof(LINE));
	return (l);
}

static void
free_line(LINE *l)
{

	l->l_next = line_freelist;
	line_freelist = l;
}

static void *
xmalloc(void *p, size_t size)
{
	void *q;

	if (!(q = (void *)realloc(p, size)))
		err(EXIT_FAILURE, "realloc");
	p = q;
	return (p);
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: col [-bfpx] [-l nline]\n");
	exit(EXIT_FAILURE);
}

static void
wrerr(void)
{

	(void)fprintf(stderr, "col: write error.\n");
	exit(EXIT_FAILURE);
}

static void
dowarn(int line)
{

	warnx("warning: can't back up %s",
		line < 0 ? "past first line" : "-- line already flushed");
}
