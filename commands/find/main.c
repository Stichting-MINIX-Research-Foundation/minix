/*	$NetBSD: main.c,v 1.28 2008/07/21 14:19:22 lukem Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
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

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <signal.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "find.h"

time_t now;			/* time find was run */
int dotfd;			/* starting directory */
int ftsoptions;			/* options for the ftsopen(3) call */
int isdeprecated;		/* using deprecated syntax */
int isdepth;			/* do directories on post-order visit */
int isoutput;			/* user specified output operator */
int issort;			/* sort directory entries */
int isxargs;			/* don't permit xargs delimiting chars */
int regcomp_flags = REG_BASIC;	/* regex compilation flags */

int main(int, char **);
static void usage(void);

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	char **p, **start;
	int ch;

	(void)time(&now);	/* initialize the time-of-day */
	(void)setlocale(LC_ALL, "");

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = show_path;
#ifdef SIGINFO
	sigaction(SIGINFO, &sa, NULL);
#endif

	/* array to hold dir list.  at most (argc - 1) elements. */
	p = start = malloc(argc * sizeof (char *));
	if (p == NULL)
		err(1, NULL);

	ftsoptions = FTS_NOSTAT | FTS_PHYSICAL;
	while ((ch = getopt(argc, argv, "HLPdEf:hsXx")) != -1)
		switch (ch) {
		case 'H':
			ftsoptions &= ~FTS_LOGICAL;
			ftsoptions |= FTS_PHYSICAL|FTS_COMFOLLOW;
			break;
		case 'L':
			ftsoptions &= ~(FTS_COMFOLLOW|FTS_PHYSICAL);
			ftsoptions |= FTS_LOGICAL;
			break;
		case 'P':
			ftsoptions &= ~(FTS_COMFOLLOW|FTS_LOGICAL);
			ftsoptions |= FTS_PHYSICAL;
			break;
		case 'd':
			isdepth = 1;
			break;
		case 'E':
			regcomp_flags = REG_EXTENDED;
			break;
		case 'f':
			*p++ = optarg;
			break;
		case 'h':
			ftsoptions &= ~FTS_PHYSICAL;
			ftsoptions |= FTS_LOGICAL;
			break;
		case 's':
			issort = 1;
			break;
		case 'X':
			isxargs = 1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case '?':
		default:
			break;
		}

	argc -= optind;
	argv += optind;

	/*
	 * Find first option to delimit the file list.  The first argument
	 * that starts with a -, or is a ! or a ( must be interpreted as a
	 * part of the find expression, according to POSIX .2.
	 */
	for (; *argv != NULL; *p++ = *argv++) {
		if (argv[0][0] == '-')
			break;
		if ((argv[0][0] == '!' || argv[0][0] == '(') &&
		    argv[0][1] == '\0')
			break;
	}

	if (p == start)
		usage();

	*p = NULL;

	if ((dotfd = open(".", O_RDONLY, 0)) == -1 ||
	    fcntl(dotfd, F_SETFD, FD_CLOEXEC) == -1)
		err(1, ".");

	exit(find_execute(find_formplan(argv), start));
}

static void
usage(void)
{

	(void)fprintf(stderr,
"usage: find [-H | -L | -P] [-dEhsXx] [-f file] file [file ...] [expression]\n");
	exit(1);
}
