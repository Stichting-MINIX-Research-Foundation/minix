/*	$NetBSD: renice.c,v 1.18 2008/07/21 14:19:25 lukem Exp $	*/

/*
 * Copyright (c) 1983, 1989, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)renice.c	8.1 (Berkeley) 6/9/93";*/
__RCSID("$NetBSD: renice.c,v 1.18 2008/07/21 14:19:25 lukem Exp $");
#endif /* not lint */

#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

static int	getnum(const char *, const char *, int *);
static int	donice(int, id_t, int, int);
static void	usage(void) __dead;

/*
 * Change the priority (nice) of processes
 * or groups of processes which are already
 * running.
 */
int
main(int argc, char **argv)
{
	int which = PRIO_PROCESS;
	int prio, errs = 0, incr = 0;
	id_t who = 0;

	argc--, argv++;
	if (argc < 2)
		usage();
	if (strcmp(*argv, "-n") == 0) {
		incr = 1;
		argc--, argv++;
		if (argc == 0)
			usage();
	}
	if (getnum("priority", *argv, &prio))
		return 1;
	argc--, argv++;
	for (; argc > 0; argc--, argv++) {
		if (strcmp(*argv, "-g") == 0) {
			which = PRIO_PGRP;
			continue;
		}
		if (strcmp(*argv, "-u") == 0) {
			which = PRIO_USER;
			continue;
		}
		if (strcmp(*argv, "-p") == 0) {
			which = PRIO_PROCESS;
			continue;
		}
		if (which == PRIO_USER) {
			struct passwd *pwd = getpwnam(*argv);
			
			if (pwd == NULL) {
				warnx("%s: unknown user", *argv);
				errs++;
				continue;
			}
			who = (id_t)pwd->pw_uid;
		} else {
			int twho;
			if (getnum("pid", *argv, &twho)) {
				errs++;
				continue;
			}
			if (twho < 0) {
				warnx("%s: bad value", *argv);
				errs++;
				continue;
			}
			who = (id_t)twho;
		}
		errs += donice(which, who, prio, incr);
	}
	return errs == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
getnum(const char *com, const char *str, int *val)
{
	long v;
	char *ep;

	errno = 0;
	v = strtol(str, &ep, 0);

	if (*ep) {
		warnx("Bad %s argument: %s", com, str);
		return 1;
	}
	if ((v == LONG_MIN || v == LONG_MAX) && errno == ERANGE) {
		warn("Invalid %s argument: %s", com, str);
		return 1;
	}

	*val = (int)v;
	return 0;
}

static int
donice(int which, id_t who, int prio, int incr)
{
	int oldprio;

	errno = 0;
	if ((oldprio = getpriority(which, who)) == -1 && errno != 0) {
		warn("%d: getpriority", who);
		return 1;
	}

	if (incr)
		prio = oldprio + prio;

	if (prio > PRIO_MAX)
		prio = PRIO_MAX;
	if (prio < PRIO_MIN)
		prio = PRIO_MIN;

	if (setpriority(which, who, prio) == -1) {
		warn("%d: setpriority", who);
		return 1;
	}
	(void)printf("%d: old priority %d, new priority %d\n",
	    who, oldprio, prio);
	return 0;
}

static void
usage(void)
{

	(void)fprintf(stderr, "Usage: %s [<priority> | -n <incr>] ",
	    getprogname());
	(void)fprintf(stderr, "[[-p] <pids>...] [-g <pgrp>...] ");
	(void)fprintf(stderr, "[-u <user>...]\n");
	exit(1);
}
