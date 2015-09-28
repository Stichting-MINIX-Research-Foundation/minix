/*	$NetBSD: time.c,v 1.22 2011/11/09 19:10:10 christos Exp $	*/

/*
 * Copyright (c) 1987, 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1987, 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)time.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: time.c,v 1.22 2011/11/09 19:10:10 christos Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <err.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ext.h"

__dead static void	usage(void);
static void	prl(long, const char *);
static void	prts(const char *, const char *, const struct timespec *,
    const char *);
static void	prtv(const char *, const char *, const struct timeval *,
    const char *);

int
main(int argc, char ** volatile argv)
{
	int pid;
	int ch, status;
	int volatile portableflag;
	int volatile lflag;
	int volatile cshflag;
	const char *decpt;
	const struct lconv *lconv;
	struct timespec before, after;
	struct rusage ru;

	(void)setlocale(LC_ALL, "");

	cshflag = lflag = portableflag = 0;
	while ((ch = getopt(argc, argv, "clp")) != -1) {
		switch (ch) {
		case 'c':
			cshflag = 1;
			portableflag = 0;
			lflag = 0;
			break;
		case 'p':
			portableflag = 1;
			cshflag = 0;
			lflag = 0;
			break;
		case 'l':
			lflag = 1;
			portableflag = 0;
			cshflag = 0;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	(void)clock_gettime(CLOCK_MONOTONIC, &before);
	switch(pid = vfork()) {
	case -1:			/* error */
		err(EXIT_FAILURE, "Vfork failed");
		/* NOTREACHED */
	case 0:				/* child */
		/* LINTED will return only on failure */
		execvp(*argv, argv);
		err((errno == ENOENT) ? 127 : 126, "Can't exec `%s'", *argv);
		/* NOTREACHED */
	}

	/* parent */
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	if ((pid = wait4(pid, &status, 0, &ru)) == -1)
		err(EXIT_FAILURE, "wait4 %d failed", pid);
	(void)clock_gettime(CLOCK_MONOTONIC, &after);
	if (!WIFEXITED(status))
		warnx("Command terminated abnormally.");
	timespecsub(&after, &before, &after);

	if ((lconv = localeconv()) == NULL ||
	    (decpt = lconv->decimal_point) == NULL)
		decpt = ".";

	if (cshflag) {
		static struct rusage null_ru;
		before.tv_sec = 0;
		before.tv_nsec = 0;
		prusage(stderr, &null_ru, &ru, &after, &before);
	} else if (portableflag) {
		prts("real ", decpt, &after, "\n");
		prtv("user ", decpt, &ru.ru_utime, "\n");
		prtv("sys  ", decpt, &ru.ru_stime, "\n");
	} else {
		prts("", decpt, &after, " real ");
		prtv("", decpt, &ru.ru_utime, " user ");
		prtv("", decpt, &ru.ru_stime, " sys\n");
	}

	if (lflag) {
		int hz = (int)sysconf(_SC_CLK_TCK);
		unsigned long long ticks;
#define SCALE(x) (long)(ticks ? x / ticks : 0)

		ticks = hz * (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
		    hz * (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000000;
		prl(ru.ru_maxrss, "maximum resident set size");
		prl(SCALE(ru.ru_ixrss), "average shared memory size");
		prl(SCALE(ru.ru_idrss), "average unshared data size");
		prl(SCALE(ru.ru_isrss), "average unshared stack size");
		prl(ru.ru_minflt, "page reclaims");
		prl(ru.ru_majflt, "page faults");
		prl(ru.ru_nswap, "swaps");
		prl(ru.ru_inblock, "block input operations");
		prl(ru.ru_oublock, "block output operations");
		prl(ru.ru_msgsnd, "messages sent");
		prl(ru.ru_msgrcv, "messages received");
		prl(ru.ru_nsignals, "signals received");
		prl(ru.ru_nvcsw, "voluntary context switches");
		prl(ru.ru_nivcsw, "involuntary context switches");
	}

	return (WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}

static void
usage(void)
{

	(void)fprintf(stderr, "Usage: %s [-clp] utility [argument ...]\n",
	    getprogname());
	exit(EXIT_FAILURE);
}

static void
prl(long val, const char *expn)
{

	(void)fprintf(stderr, "%10ld  %s\n", val, expn);
}

static void
prts(const char *pre, const char *decpt, const struct timespec *ts,
    const char *post)
{

	(void)fprintf(stderr, "%s%9lld%s%02ld%s", pre, (long long)ts->tv_sec,
	    decpt, (long)ts->tv_nsec / 10000000, post);
}

static void
prtv(const char *pre, const char *decpt, const struct timeval *tv,
    const char *post)
{

	(void)fprintf(stderr, "%s%9lld%s%02ld%s", pre, (long long)tv->tv_sec,
	    decpt, (long)tv->tv_usec / 10000, post);
}
