/*	$NetBSD: tput.c,v 1.22 2011/10/04 12:23:14 roy Exp $	*/

/*-
 * Copyright (c) 1980, 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tput.c	8.3 (Berkeley) 4/28/95";
#endif
__RCSID("$NetBSD: tput.c,v 1.22 2011/10/04 12:23:14 roy Exp $");
#endif /* not lint */

#include <termios.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termcap.h>
#include <unistd.h>

static int    outc(int);
static void   usage(void) __dead;
static char **process(const char *, const char *, char **);

int
main(int argc, char **argv)
{
	int ch, exitval, n;
	char *term;
	const char *p, *s;
	size_t pl;

	term = NULL;
	while ((ch = getopt(argc, argv, "T:")) != -1)
		switch(ch) {
		case 'T':
			term = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!term && !(term = getenv("TERM")))
		errx(2, "No terminal type specified and no TERM "
		    "variable set in the environment.");
	setupterm(term, 0, NULL);
	for (exitval = 0; (p = *argv) != NULL; ++argv) {
		switch (*p) {
		case 'c':
			if (!strcmp(p, "clear"))
				p = "clear";
			break;
		case 'i':
			if (!strcmp(p, "init")) {
				s = tigetstr("is1");
				if (s != NULL)
					tputs(s, 0, outc);
				p = "is2";
			}
			break;
		case 'l':
			if (!strcmp(p, "longname")) {
				(void)printf("%s\n", longname());
				continue;
			}
			break;
		case 'r':
			if (!strcmp(p, "reset")) {
				s = tigetstr("rs1");
				if (s != NULL)
					tputs(s, 0, outc);
				p = "rs2";
			}
			break;
		}
		pl = strlen(p);
		if (((s = tigetstr(p)) != NULL && s != (char *)-1) ||
		    (pl <= 2 && (s = tgetstr(p, NULL)) != NULL))
			argv = process(p, s, argv);
		else if ((((n = tigetnum(p)) != -1 && n != -2 ) ||
			   (pl <= 2 && (n = tgetnum(p)) != -1)))
			(void)printf("%d\n", n);
		else {
			exitval = tigetflag(p);
			if (exitval == -1) {
				if (pl <= 2)
					exitval = !tgetflag(p);
				else
					exitval = 1;
			} else
				exitval = !exitval;
		}

		if (argv == NULL)
			break;
	}
	return argv ? exitval : 2;
}

static char **
process(const char *cap, const char *str, char **argv)
{
	static const char errfew[] =
	    "Not enough arguments (%d) for capability `%s'";
	static const char erresc[] =
	    "Unknown %% escape `%c' for capability `%s'";
	char c, l;
	const char *p;
	int arg_need, p1, p2, p3, p4, p5, p6, p7, p8, p9;

	/* Count how many values we need for this capability. */
	arg_need = 0;
	p = str;
	while ((c = *p++) != '\0') {
		if (c != '%')
			continue;
		c = *p++;
		if (c == '\0')
			break;
		if (c != 'p')
			continue;
		c = *p++;
		if (c < '1' || c > '9')
			errx(2, erresc, c, cap);
		l = c - '0';
		if (l > arg_need)
			arg_need = l;
	}
	
#define NEXT_ARG							      \
	{								      \
		if (*++argv == NULL || *argv[0] == '\0')		      \
			errx(2, errfew, 1, cap);			      \
	}

	if (arg_need > 0) {
		NEXT_ARG;
		p1 = atoi(*argv);
	} else
		p1 = 0;
	if (arg_need > 1) {
		NEXT_ARG;
		p2 = atoi(*argv);
	} else
		p2 = 0;
	if (arg_need > 2) {
		NEXT_ARG;
		p3 = atoi(*argv);
	} else
		p3 = 0;
	if (arg_need > 3) {
		NEXT_ARG;
		p4 = atoi(*argv);
	} else
		p4 = 0;
	if (arg_need > 4) {
		NEXT_ARG;
		p5 = atoi(*argv);
	} else
		p5 = 0;
	if (arg_need > 5) {
		NEXT_ARG;
		p6 = atoi(*argv);
	} else
		p6 = 0;
	if (arg_need > 6) {
		NEXT_ARG;
		p7 = atoi(*argv);
	} else
		p7 = 0;
	if (arg_need > 7) {
		NEXT_ARG;
		p8 = atoi(*argv);
	} else
		p8 = 0;
	if (arg_need > 8) {
		NEXT_ARG;
		p9 = atoi(*argv);
	} else
		p9 = 0;

	/* And print them. */
	(void)tputs(tparm(str, p1, p2, p3, p4, p5, p6, p7, p8, p9), 0, outc);
	return argv;
}

static int
outc(int c)
{
	return putchar(c);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-T term] attribute [attribute-args] ...\n",
	    getprogname());
	exit(2);
}
