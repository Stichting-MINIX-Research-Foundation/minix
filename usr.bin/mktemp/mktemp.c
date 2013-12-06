/* $NetBSD: mktemp.c,v 1.12 2012/11/03 13:34:08 christos Exp $ */

/*-
 * Copyright (c) 1994, 1995, 1996, 1998 Peter Wemm <peter@netplex.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * This program was originally written long ago, originally for a non
 * BSD-like OS without mkstemp().  It's been modified over the years
 * to use mkstemp() rather than the original O_CREAT|O_EXCL/fstat/lstat
 * etc style hacks.
 * A cleanup, misc options and mkdtemp() calls were added to try and work
 * more like the OpenBSD version - which was first to publish the interface.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#include <sys/types.h>
#include <err.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__RCSID) && !defined(__lint)
__RCSID("$NetBSD: mktemp.c,v 1.12 2012/11/03 13:34:08 christos Exp $");
#endif /* !__lint */

static void usage(void) __dead;

int
main(int argc, char **argv)
{
	int c, fd, ret;
	char *tmpdir;
	const char *prefix;
	char *name;
	int dflag, qflag, tflag, uflag;

	setprogname(*argv);
	ret = dflag = qflag = tflag = uflag = 0;
	tmpdir = NULL;
	prefix = "mktemp";
	name = NULL;

	while ((c = getopt(argc, argv, "dp:qt:u")) != -1)
		switch (c) {
		case 'd':
			dflag++;
			break;

		case 'p':
			tmpdir = optarg;
			break;
			
		case 'q':
			qflag++;
			break;

		case 't':
			prefix = optarg;
			tflag++;
			break;

		case 'u':
			uflag++;
			break;

		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (tflag == 0 && argc < 1)
		tflag = 1;

	if (tflag) {
		if (tmpdir == NULL)
			tmpdir = getenv("TMPDIR");
		if (tmpdir == NULL)
			(void)asprintf(&name, "%s%s.XXXXXXXX", _PATH_TMP,
			    prefix);
		else
			(void)asprintf(&name, "%s/%s.XXXXXXXX", tmpdir, prefix);
		/* if this fails, the program is in big trouble already */
		if (name == NULL) {
			if (qflag)
				return 1;
			else
				errx(1, "Cannot generate template");
		}
	} else if (argc < 1) {
		usage();
	}
		
	/* generate all requested files */
	while (name != NULL || argc > 0) {
		if (name == NULL) {
			if (tmpdir)
				(void)asprintf(&name, "%s/%s",
				    tmpdir, argv[0]);
			else
				name = strdup(argv[0]);
			argv++;
			argc--;
		}

		if (dflag) {
			if (mkdtemp(name) == NULL) {
				ret = 1;
				if (!qflag)
					warn("mkdtemp failed on %s", name);
			} else {
				(void)printf("%s\n", name);
				if (uflag)
					(void)rmdir(name);
			}
		} else {
			fd = mkstemp(name);
			if (fd < 0) {
				ret = 1;
				if (!qflag)
					warn("mkstemp failed on %s", name);
			} else {
				(void)close(fd);
				if (uflag)
					(void)unlink(name);
				(void)printf("%s\n", name);
			}
		}
		if (name)
			free(name);
		name = NULL;
	}
	return ret;
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-dqu] [-p <tmpdir>] {-t prefix | template ...}\n",
	    getprogname());
	exit (1);
}
