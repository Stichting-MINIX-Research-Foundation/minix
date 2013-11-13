/*	$NetBSD: ul.c,v 1.16 2012/03/20 20:34:59 matt Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ul.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: ul.c,v 1.16 2012/03/20 20:34:59 matt Exp $");
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>

#define	IESC	'\033'
#define	SO	'\016'
#define	SI	'\017'
#define	HFWD	'9'
#define	HREV	'8'
#define	FREV	'7'
#define	MAXBUF	512

#define	NORMAL	000
#define	ALTSET	001	/* Reverse */
#define	SUPERSC	002	/* Dim */
#define	SUBSC	004	/* Dim | Ul */
#define	UNDERL	010	/* Ul */
#define	BOLD	020	/* Bold */

struct tinfo *info;
int	must_overstrike;
const char *CURS_UP, *CURS_RIGHT, *CURS_LEFT,
	*ENTER_STANDOUT, *EXIT_STANDOUT, *ENTER_UNDERLINE, *EXIT_UNDERLINE,
	*ENTER_DIM, *ENTER_BOLD, *ENTER_REVERSE, *UNDER_CHAR, *EXIT_ATTRIBUTES;

struct	CHAR	{
	char	c_mode;
	char	c_char;
} ;

struct	CHAR	obuf[MAXBUF];
int	col, maxcol;
int	mode;
int	halfpos;
int	upln;
int	iflag;

int	main __P((int, char **));
void	filter __P((FILE *));
void	flushln __P((void));
void	fwd __P((void));
void	iattr __P((void));
void	initbuf __P((void));
void	initcap __P((void));
void	outc __P((int));
int	outchar __P((int));
void	overstrike __P((void));
void	reverse __P((void));
void	setulmode __P((int));


#define	PRINT(s)	if (s == NULL) /* void */; else tputs(s, 1, outchar)

int
main(int argc, char **argv)
{
	int c;
	const char *termtype;
	FILE *f;

	termtype = getenv("TERM");
	if (termtype == NULL || (argv[0][0] == 'c' && !isatty(1)))
		termtype = "lpr";
	while ((c=getopt(argc, argv, "it:T:")) != -1)
		switch(c) {

		case 't':
		case 'T': /* for nroff compatibility */
				termtype = optarg;
			break;
		case 'i':
			iflag = 1;
			break;

		default:
			fprintf(stderr,
				"usage: %s [ -i ] [ -tTerm ] file...\n",
				argv[0]);
			exit(1);
		}

	setupterm(termtype, 0, NULL);
	if ((over_strike && enter_bold_mode == NULL) ||
	    (transparent_underline && enter_underline_mode == NULL &&
	     underline_char == NULL))
	initbuf();
	if (optind == argc)
		filter(stdin);
	else for (; optind<argc; optind++) {
		f = fopen(argv[optind],"r");
		if (f == NULL) {
			perror(argv[optind]);
			exit(1);
		}

		filter(f);
		fclose(f);
	}
	exit(0);
}

void
filter(FILE *f)
{
	int c;

	while ((c = getc(f)) != EOF) switch(c) {

	case '\b':
		if (col > 0)
			col--;
		continue;

	case '\t':
		col = (col+8) & ~07;
		if (col > maxcol)
			maxcol = col;
		continue;

	case '\r':
		col = 0;
		continue;

	case SO:
		mode |= ALTSET;
		continue;

	case SI:
		mode &= ~ALTSET;
		continue;

	case IESC:
		switch (c = getc(f)) {

		case HREV:
			if (halfpos == 0) {
				mode |= SUPERSC;
				halfpos--;
			} else if (halfpos > 0) {
				mode &= ~SUBSC;
				halfpos--;
			} else {
				halfpos = 0;
				reverse();
			}
			continue;

		case HFWD:
			if (halfpos == 0) {
				mode |= SUBSC;
				halfpos++;
			} else if (halfpos < 0) {
				mode &= ~SUPERSC;
				halfpos++;
			} else {
				halfpos = 0;
				fwd();
			}
			continue;

		case FREV:
			reverse();
			continue;

		default:
			fprintf(stderr,
				"Unknown escape sequence in input: %o, %o\n",
				IESC, c);
			exit(1);
		}
		continue;

	case '_':
		if (obuf[col].c_char)
			obuf[col].c_mode |= UNDERL | mode;
		else
			obuf[col].c_char = '_';
	case ' ':
		col++;
		if (col > maxcol)
			maxcol = col;
		continue;

	case '\n':
		flushln();
		continue;

	case '\f':
		flushln();
		putchar('\f');
		continue;

	default:
		if (c < ' ')	/* non printing */
			continue;
		if (obuf[col].c_char == '\0') {
			obuf[col].c_char = c;
			obuf[col].c_mode = mode;
		} else if (obuf[col].c_char == '_') {
			obuf[col].c_char = c;
			obuf[col].c_mode |= UNDERL|mode;
		} else if (obuf[col].c_char == c)
			obuf[col].c_mode |= BOLD|mode;
		else
			obuf[col].c_mode = mode;
		col++;
		if (col > maxcol)
			maxcol = col;
		continue;
	}
	if (maxcol)
		flushln();
}

void
flushln(void)
{
	int lastmode;
	int i;
	int hadmodes = 0;

	lastmode = NORMAL;
	for (i=0; i<maxcol; i++) {
		if (obuf[i].c_mode != lastmode) {
			hadmodes++;
			setulmode(obuf[i].c_mode);
			lastmode = obuf[i].c_mode;
		}
		if (obuf[i].c_char == '\0') {
			if (upln) {
				PRINT(cursor_right);
			}
			else {
				outc(' ');
			}
		} else
			outc(obuf[i].c_char);
	}
	if (lastmode != NORMAL) {
		setulmode(0);
	}
	if (must_overstrike && hadmodes)
		overstrike();
	putchar('\n');
	if (iflag && hadmodes)
		iattr();
	(void)fflush(stdout);
	if (upln)
		upln--;
	initbuf();
}

/*
 * For terminals that can overstrike, overstrike underlines and bolds.
 * We don't do anything with halfline ups and downs, or Greek.
 */
void
overstrike(void)
{
	int i;
	char lbuf[256];
	char *cp = lbuf;
	int hadbold=0;

	/* Set up overstrike buffer */
	for (i=0; i<maxcol; i++)
		switch (obuf[i].c_mode) {
		case NORMAL:
		default:
			*cp++ = ' ';
			break;
		case UNDERL:
			*cp++ = '_';
			break;
		case BOLD:
			*cp++ = obuf[i].c_char;
			hadbold=1;
			break;
		}
	putchar('\r');
	for (*cp=' '; *cp==' '; cp--)
		*cp = 0;
	for (cp=lbuf; *cp; cp++)
		putchar(*cp);
	if (hadbold) {
		putchar('\r');
		for (cp=lbuf; *cp; cp++)
			putchar(*cp=='_' ? ' ' : *cp);
		putchar('\r');
		for (cp=lbuf; *cp; cp++)
			putchar(*cp=='_' ? ' ' : *cp);
	}
}

void
iattr(void)
{
	int i;
	char lbuf[256];
	char *cp = lbuf;

	for (i=0; i<maxcol; i++)
		switch (obuf[i].c_mode) {
		case NORMAL:	*cp++ = ' '; break;
		case ALTSET:	*cp++ = 'g'; break;
		case SUPERSC:	*cp++ = '^'; break;
		case SUBSC:	*cp++ = 'v'; break;
		case UNDERL:	*cp++ = '_'; break;
		case BOLD:	*cp++ = '!'; break;
		default:	*cp++ = 'X'; break;
		}
	for (*cp=' '; *cp==' '; cp--)
		*cp = 0;
	for (cp=lbuf; *cp; cp++)
		putchar(*cp);
	putchar('\n');
}

void
initbuf(void)
{

	memset((char *)obuf, 0, sizeof (obuf));	/* depends on NORMAL == 0 */
	col = 0;
	maxcol = 0;
	mode &= ALTSET;
}

void
fwd(void)
{
	int oldcol, oldmax;

	oldcol = col;
	oldmax = maxcol;
	flushln();
	col = oldcol;
	maxcol = oldmax;
}

void
reverse(void)
{
	upln++;
	fwd();
	PRINT(cursor_up);
	PRINT(cursor_up);
	upln++;
}

int
outchar(int c)
{
	return (putchar(c & 0177));
}

static int curmode = 0;

void
outc(int c)
{
	putchar(c);
	if (underline_char && !enter_underline_mode && (curmode & UNDERL)) {
		if (cursor_left)
			PRINT(cursor_left);
		else
			putchar('\b');
		PRINT(underline_char);
	}
}

void
setulmode(int newmode)
{
	if (!iflag) {
		if (curmode != NORMAL && newmode != NORMAL)
			setulmode(NORMAL);
		switch (newmode) {
		case NORMAL:
			switch(curmode) {
			case NORMAL:
				break;
			case UNDERL:
				if (enter_underline_mode)
					PRINT(exit_underline_mode);
				else
					PRINT(exit_standout_mode);
				break;
			default:
				/* This includes standout */
				if (exit_attribute_mode)
					PRINT(exit_attribute_mode);
				else
					PRINT(exit_standout_mode);
				break;
			}
			break;
		case ALTSET:
			if (enter_reverse_mode)
				PRINT(enter_reverse_mode);
			else
				PRINT(enter_standout_mode);
			break;
		case SUPERSC:
			/*
			 * This only works on a few terminals.
			 * It should be fixed.
			 */
			PRINT(enter_underline_mode);
			PRINT(enter_dim_mode);
			break;
		case SUBSC:
			if (enter_dim_mode)
				PRINT(enter_dim_mode);
			else
				PRINT(enter_standout_mode);
			break;
		case UNDERL:
			if (enter_underline_mode)
				PRINT(enter_underline_mode);
			else
				PRINT(enter_standout_mode);
			break;
		case BOLD:
			if (enter_bold_mode)
				PRINT(enter_bold_mode);
			else
				PRINT(enter_reverse_mode);
			break;
		default:
			/*
			 * We should have some provision here for multiple modes
			 * on at once.  This will have to come later.
			 */
			PRINT(enter_standout_mode);
			break;
		}
	}
	curmode = newmode;
}
