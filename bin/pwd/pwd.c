/* $NetBSD: pwd.c,v 1.22 2011/08/29 14:51:19 joerg Exp $ */

/*
 * Copyright (c) 1991, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1991, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pwd.c	8.3 (Berkeley) 4/1/94";
#else
__RCSID("$NetBSD: pwd.c,v 1.22 2011/08/29 14:51:19 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *getcwd_logical(void);
__dead static void usage(void);

/*
 * Note that EEE Std 1003.1, 2003 requires that the default be -L.
 * This is inconsistent with the historic behaviour of everything
 * except the ksh builtin.
 * To avoid breaking scripts the default has been kept as -P.
 * (Some scripts run /bin/pwd in order to get 'pwd -P'.)
 */

int
main(int argc, char *argv[])
{
	int ch, lFlag;
	const char *p;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	lFlag = 0;
	while ((ch = getopt(argc, argv, "LP")) != -1) {
		switch (ch) {
		case 'L':
			lFlag = 1;
			break;
		case 'P':
			lFlag = 0;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (lFlag)
		p = getcwd_logical();
	else
		p = NULL;
	if (p == NULL)
		p = getcwd(NULL, 0);

	if (p == NULL)
		err(EXIT_FAILURE, NULL);

	(void)printf("%s\n", p);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

static char *
getcwd_logical(void)
{
	char *pwd;
	struct stat s_pwd, s_dot;

	/* Check $PWD -- if it's right, it's fast. */
	pwd = getenv("PWD");
	if (pwd == NULL)
		return NULL;
	if (pwd[0] != '/')
		return NULL;
	if (strstr(pwd, "/./") != NULL)
		return NULL;
	if (strstr(pwd, "/../") != NULL)
		return NULL;
	if (stat(pwd, &s_pwd) == -1 || stat(".", &s_dot) == -1)
		return NULL;
	if (s_pwd.st_dev != s_dot.st_dev || s_pwd.st_ino != s_dot.st_ino)
		return NULL;
	return pwd;
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-LP]\n", getprogname());
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
