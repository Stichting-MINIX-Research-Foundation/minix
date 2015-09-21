/*	$NetBSD: cmp.c,v 1.18 2011/08/29 14:14:11 joerg Exp $	*/

/*
 * Copyright (c) 1987, 1990, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1987, 1990, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)cmp.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: cmp.c,v 1.18 2011/08/29 14:14:11 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

#include "extern.h"

int	lflag, sflag;

__dead static void usage(void);

int
main(int argc, char *argv[])
{
	struct stat sb1, sb2;
	off_t skip1 = 0, skip2 = 0;
	int ch, fd1, fd2, special;
	const char *file1, *file2;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "ls")) != -1)
		switch (ch) {
		case 'l':		/* print all differences */
			lflag = 1;
			break;
		case 's':		/* silent run */
			sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (lflag && sflag)
		errx(ERR_EXIT, "only one of -l and -s may be specified");

	if (argc < 2 || argc > 4)
		usage();

	/* Backward compatibility -- handle "-" meaning stdin. */
	special = 0;
	if (strcmp(file1 = argv[0], "-") == 0) {
		special = 1;
		fd1 = 0;
		file1 = "stdin";
	}
	else if ((fd1 = open(file1, O_RDONLY, 0)) < 0) {
		if (!sflag)
			warn("%s", file1);
		exit(ERR_EXIT);
	}
	if (strcmp(file2 = argv[1], "-") == 0) {
		if (special)
			errx(ERR_EXIT,
				"standard input may only be specified once");
		special = 1;
		fd2 = 0;
		file2 = "stdin";
	}
	else if ((fd2 = open(file2, O_RDONLY, 0)) < 0) {
		if (!sflag)
			warn("%s", file2);
		exit(ERR_EXIT);
	}

	if (argc > 2) {
		char *ep;

		errno = 0;
		skip1 = strtoq(argv[2], &ep, 0);
		if (errno || ep == argv[2])
			usage();

		if (argc == 4) {
			skip2 = strtoq(argv[3], &ep, 0);
			if (errno || ep == argv[3])
				usage();
		}
	}

	if (!special) {
		if (fstat(fd1, &sb1))
			err(ERR_EXIT, "%s", file1);
		if (!S_ISREG(sb1.st_mode))
			special = 1;
		else {
			if (fstat(fd2, &sb2))
				err(ERR_EXIT, "%s", file2);
			if (!S_ISREG(sb2.st_mode))
				special = 1;
		}
	}

	if (special)
		c_special(fd1, file1, skip1, fd2, file2, skip2);
	else
		c_regular(fd1, file1, skip1, sb1.st_size,
		    fd2, file2, skip2, sb2.st_size);
	exit(0);
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: cmp [-l | -s] file1 file2 [skip1 [skip2]]\n");
	exit(ERR_EXIT);
}
