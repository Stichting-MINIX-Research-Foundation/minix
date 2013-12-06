/*	$NetBSD: tput.c,v 1.26 2013/02/05 11:31:56 roy Exp $	*/

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
__RCSID("$NetBSD: tput.c,v 1.26 2013/02/05 11:31:56 roy Exp $");
#endif /* not lint */

#include <termios.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term_private.h>
#include <term.h>
#include <unistd.h>

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
					putp(s);
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
					putp(s);
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
	    "Unknown %% escape (%s) for capability `%s'";
	static const char errnum[] =
	    "Expected a numeric argument [%d] (%s) for capability `%s'";
	static const char errcharlong[] = 
	    "Platform does not fit a string into a long for capability '%s'";
	int i, nparams, piss[TPARM_MAX];
	long nums[TPARM_MAX];
	char *strs[TPARM_MAX], *tmp;

	/* Count how many values we need for this capability. */
	errno = 0;
	memset(&piss, 0, sizeof(piss));
	nparams = _ti_parm_analyse(str, piss, TPARM_MAX);
	if (errno == EINVAL)
		errx(2, erresc, str, cap);

	/* Create our arrays of integers and strings */
	for (i = 0; i < nparams; i++) {
		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, nparams, cap);
		if (piss[i]) {
			if (sizeof(char *) > sizeof(long) /* CONSTCOND */)
				errx(2, errcharlong, cap);
			strs[i] = *argv;
		} else {
			errno = 0;
			nums[i] = strtol(*argv, &tmp, 0);
			if ((errno == ERANGE && 
			    (nums[i] == LONG_MIN || nums[i] == LONG_MAX)) ||
			    (errno != 0 && nums[i] == 0) ||
			    tmp == str ||
			    *tmp != '\0')
				errx(2, errnum, i + 1, *argv, cap);
		}
	}

	/* And output */
#define p(i)	(i <= nparams ? \
		    (piss[i - 1] ? (long)strs[i - 1] : nums[i - 1]) : 0)
	putp(tparm(str, p(1), p(2), p(3), p(4), p(5), p(6), p(7), p(8), p(9)));

	return argv;
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-T term] attribute [attribute-args] ...\n",
	    getprogname());
	exit(2);
}
