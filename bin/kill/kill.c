/* $NetBSD: kill.c,v 1.27 2011/08/29 14:51:18 joerg Exp $ */

/*
 * Copyright (c) 1988, 1993, 1994
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
#if !defined(lint) && !defined(SHELL)
__COPYRIGHT("@(#) Copyright (c) 1988, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)kill.c	8.4 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: kill.c,v 1.27 2011/08/29 14:51:18 joerg Exp $");
#endif
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <locale.h>
#include <sys/ioctl.h>

#ifdef SHELL            /* sh (aka ash) builtin */
int killcmd(int, char *argv[]);
#define main killcmd
#include "../../bin/sh/bltin/bltin.h"
#endif /* SHELL */ 

__dead static void nosig(char *);
static void printsignals(FILE *);
static int signame_to_signum(char *);
__dead static void usage(void);

int
main(int argc, char *argv[])
{
	int errors;
	intmax_t numsig, pid;
	char *ep;

	setprogname(argv[0]);
	setlocale(LC_ALL, "");
	if (argc < 2)
		usage();

	numsig = SIGTERM;

	argc--, argv++;
	if (strcmp(*argv, "-l") == 0) {
		argc--, argv++;
		if (argc > 1)
			usage();
		if (argc == 1) {
			if (isdigit((unsigned char)**argv) == 0)
				usage();
			numsig = strtoimax(*argv, &ep, 10);
			/* check for correctly parsed number */
			if (*ep != '\0' || numsig == INTMAX_MIN || numsig == INTMAX_MAX) {
				errx(EXIT_FAILURE, "illegal signal number: %s",
						*argv);
				/* NOTREACHED */
			}
			if (numsig >= 128)
				numsig -= 128;
			/* and whether it fits into signals range */
			if (numsig <= 0 || numsig >= NSIG)
				nosig(*argv);
			printf("%s\n", sys_signame[(int) numsig]);
			exit(0);
		}
		printsignals(stdout);
		exit(0);
	}

	if (!strcmp(*argv, "-s")) {
		argc--, argv++;
		if (argc < 1) {
			warnx("option requires an argument -- s");
			usage();
		}
		if (strcmp(*argv, "0")) {
			if ((numsig = signame_to_signum(*argv)) < 0)
				nosig(*argv);
		} else
			numsig = 0;
		argc--, argv++;
	} else if (**argv == '-') {
		char *sn = *argv + 1;
		if (isalpha((unsigned char)*sn)) {
			if ((numsig = signame_to_signum(sn)) < 0)
				nosig(sn);
		} else if (isdigit((unsigned char)*sn)) {
			numsig = strtoimax(sn, &ep, 10);
			/* check for correctly parsed number */
			if (*ep || numsig == INTMAX_MIN || numsig == INTMAX_MAX ) {
				errx(EXIT_FAILURE, "illegal signal number: %s",
						sn);
				/* NOTREACHED */
			}
			/* and whether it fits into signals range */
			if (numsig < 0 || numsig >= NSIG)
				nosig(sn);
		} else
			nosig(sn);
		argc--, argv++;
	}

	if (argc == 0)
		usage();

	for (errors = 0; argc; argc--, argv++) {
#ifdef SHELL
		extern int getjobpgrp(const char *);
		if (*argv[0] == '%') {
			pid = getjobpgrp(*argv);
			if (pid == 0) {
				warnx("illegal job id: %s", *argv);
				errors = 1;
				continue;
			}
		} else 
#endif
		{
			pid = strtoimax(*argv, &ep, 10);
			/* make sure the pid is a number and fits into pid_t */
			if (!**argv || *ep || pid == INTMAX_MIN ||
				pid == INTMAX_MAX || pid != (pid_t) pid) {

				warnx("illegal process id: %s", *argv);
				errors = 1;
				continue;
			}
		}
		if (kill((pid_t) pid, (int) numsig) == -1) {
			warn("%s", *argv);
			errors = 1;
		}
#ifdef SHELL
		/* Wakeup the process if it was suspended, so it can
		   exit without an explicit 'fg'. */
		if (numsig == SIGTERM || numsig == SIGHUP)
			kill((pid_t) pid, SIGCONT);
#endif
	}

	exit(errors);
	/* NOTREACHED */
}

static int
signame_to_signum(char *sig)
{
	int n;

	if (strncasecmp(sig, "sig", 3) == 0)
		sig += 3;
	for (n = 1; n < NSIG; n++) {
		if (!strcasecmp(sys_signame[n], sig))
			return (n);
	}
	return (-1);
}

static void
nosig(char *name)
{

	warnx("unknown signal %s; valid signals:", name);
	printsignals(stderr);
	exit(1);
	/* NOTREACHED */
}

static void
printsignals(FILE *fp)
{
	int sig;
	int len, nl;
	const char *name;
	int termwidth = 80;

	if (isatty(fileno(fp))) {
		struct winsize win;
		if (ioctl(fileno(fp), TIOCGWINSZ, &win) == 0 && win.ws_col > 0)
			termwidth = win.ws_col;
	}

	for (len = 0, sig = 1; sig < NSIG; sig++) {
		name = sys_signame[sig];
		nl = 1 + strlen(name);

		if (len + nl >= termwidth) {
			fprintf(fp, "\n");
			len = 0;
		} else
			if (len != 0)
				fprintf(fp, " ");
		len += nl;
		fprintf(fp, "%s", name);
	}
	if (len != 0)
		fprintf(fp, "\n");
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-s signal_name] pid ...\n"
	    "       %s -l [exit_status]\n"
	    "       %s -signal_name pid ...\n"
	    "       %s -signal_number pid ...\n",
	    getprogname(), getprogname(), getprogname(), getprogname());
	exit(1);
	/* NOTREACHED */
}
