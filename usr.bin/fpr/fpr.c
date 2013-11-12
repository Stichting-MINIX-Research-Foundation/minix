/*	$NetBSD: fpr.c,v 1.9 2011/09/04 20:26:17 joerg Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Corbett.
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
#endif				/* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)fpr.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: fpr.c,v 1.9 2011/09/04 20:26:17 joerg Exp $");
#endif				/* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#define BLANK ' '
#define TAB '\t'
#define NUL '\000'
#define FF '\f'
#define BS '\b'
#define CR '\r'
#define VTAB '\013'
#define EOL '\n'

#define TRUE 1
#define FALSE 0

#define MAXCOL 170
#define TABSIZE 8
#define INITWIDTH 8

typedef
struct column {
	int     count;
	int     width;
	char   *str;
}
        COLUMN;

static char    cc;
static char    saved;
static int     length;
static char   *text;
static int     highcol;
static COLUMN *line;
static int     maxpos;
static int     maxcol;

static void	flush(void);
static void	get_text(void);
static void	init(void);
__dead static void	nospace(void);
static void	savech(int);

int
main(int argc, char **argv)
{
	int ch;
	char ateof;
	int i;
	int errorcount;

	init();
	errorcount = 0;
	ateof = FALSE;

	switch (ch = getchar()) {
	case EOF:
		exit(0);
	case EOL:
		cc = NUL;
		ungetc((int) EOL, stdin);
		break;
	case BLANK:
		cc = NUL;
		break;
	case '1':
		cc = FF;
		break;
	case '0':
		cc = EOL;
		break;
	case '+':
		cc = CR;
		break;
	default:
		errorcount = 1;
		cc = NUL;
		ungetc(ch, stdin);
		break;
	}

	while (!ateof) {
		get_text();
		switch (ch = getchar()) {
		case EOF:
			flush();
			ateof = TRUE;
			break;
		case EOL:
			flush();
			cc = NUL;
			ungetc((int) EOL, stdin);
			break;
		case BLANK:
			flush();
			cc = NUL;
			break;
		case '1':
			flush();
			cc = FF;
			break;
		case '0':
			flush();
			cc = EOL;
			break;
		case '+':
			for (i = 0; i < length; i++)
				savech(i);
			break;
		default:
			errorcount++;
			flush();
			cc = NUL;
			ungetc(ch, stdin);
			break;
		}
	}

	if (errorcount)
		fprintf(stderr, "Illegal carriage control - %d line%s.\n",
		    errorcount, errorcount == 1 ? "" : "s");

	exit(0);
}

static void
init(void)
{
	COLUMN *cp;
	COLUMN *cend;
	char *sp;

	length = 0;
	maxpos = MAXCOL;
	sp = malloc((unsigned) maxpos);
	if (sp == NULL)
		nospace();
	text = sp;

	highcol = -1;
	maxcol = MAXCOL;
	line = calloc(maxcol, sizeof(COLUMN));
	if (line == NULL)
		nospace();
	cp = line;
	cend = line + (maxcol - 1);
	while (cp <= cend) {
		cp->width = INITWIDTH;
		sp = calloc(INITWIDTH, sizeof(char));
		if (sp == NULL)
			nospace();
		cp->str = sp;
		cp++;
	}
}

static void
get_text(void)
{
	int i;
	char ateol;
	int ch;
	int pos;
	char *n;

	i = 0;
	ateol = FALSE;

	while (!ateol) {
		switch (ch = getchar()) {
		case EOL:
		case EOF:
			ateol = TRUE;
			break;
		case TAB:
			pos = (1 + i / TABSIZE) * TABSIZE;
			if (pos > maxpos) {
				n = realloc(text, (unsigned)(pos + 10));
				if (n == NULL)
					nospace();
				text = n;
				maxpos = pos + 10;
			}
			while (i < pos) {
				text[i] = BLANK;
				i++;
			}
			break;
		case BS:
			if (i > 0) {
				i--;
				savech(i);
			}
			break;
		case CR:
			while (i > 0) {
				i--;
				savech(i);
			}
			break;
		case FF:
		case VTAB:
			flush();
			cc = ch;
			i = 0;
			break;
		default:
			if (i >= maxpos) {
				n = realloc(text, (unsigned)(i + 10));
				if (n == NULL)
					nospace();
				maxpos = i + 10;
			}
			text[i] = ch;
			i++;
			break;
		}
	}

	length = i;
}

static void
savech(int col)
{
	char ch;
	int oldmax;
	COLUMN *cp;
	COLUMN *cend;
	char *sp;
	int newcount;
	COLUMN *newline;

	ch = text[col];
	if (ch == BLANK)
		return;

	saved = TRUE;

	if (col >= highcol)
		highcol = col;

	if (col >= maxcol) {
		newline = realloc(line, (unsigned) (col + 10) * sizeof(COLUMN));
		if (newline == NULL)
			nospace();
		line = newline;
		oldmax = maxcol;
		maxcol = col + 10;
		cp = line + oldmax;
		cend = line + (maxcol - 1);
		while (cp <= cend) {
			cp->width = INITWIDTH;
			cp->count = 0;
			sp = calloc(INITWIDTH, sizeof(char));
			if (sp == NULL)
				nospace();
			cp->str = sp;
			cp++;
		}
	}
	cp = line + col;
	newcount = cp->count + 1;
	if (newcount > cp->width) {
		cp->width = newcount;
		sp = realloc(cp->str, (unsigned) newcount * sizeof(char));
		if (sp == NULL)
			nospace();
		cp->str = sp;
	}
	cp->count = newcount;
	cp->str[newcount - 1] = ch;
}

static void
flush(void)
{
	int i;
	int anchor;
	int height;
	int j;

	if (cc != NUL)
		putchar(cc);

	if (!saved) {
		i = length;
		while (i > 0 && text[i - 1] == BLANK)
			i--;
		length = i;
		for (i = 0; i < length; i++)
			putchar(text[i]);
		putchar(EOL);
		return;
	}
	for (i = 0; i < length; i++)
		savech(i);

	anchor = 0;
	while (anchor <= highcol) {
		height = line[anchor].count;
		if (height == 0) {
			putchar(BLANK);
			anchor++;
		} else if (height == 1) {
			putchar(*(line[anchor].str));
			line[anchor].count = 0;
			anchor++;
		} else {
			i = anchor;
			while (i < highcol && line[i + 1].count > 1)
				i++;
			for (j = anchor; j <= i; j++) {
				height = line[j].count - 1;
				putchar(line[j].str[height]);
				line[j].count = height;
			}
			for (j = anchor; j <= i; j++)
				putchar(BS);
		}
	}

	putchar(EOL);
	highcol = -1;
}

static void
nospace(void)
{
	errx(1, "Storage limit exceeded.");
}
