/*	$NetBSD: basename.c,v 1.15 2011/08/29 14:24:03 joerg Exp $	*/

/*-
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
static char sccsid[] = "@(#)basename.c	8.4 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: basename.c,v 1.15 2011/08/29 14:24:03 joerg Exp $");
#endif /* not lint */

#include <err.h>
#include <libgen.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__dead static void usage(void);

int
main(int argc, char **argv)
{
	char *p;
	int ch;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1 && argc != 2)
		usage();

	if (**argv == '\0') {
		(void)printf("\n");
		exit(0);
	}
	if ((p = basename(*argv)) == NULL)
		err(1, "%s", *argv);
	if (*++argv != '\0') {
		int suffixlen, stringlen, off;

		suffixlen = strlen(*argv);
		stringlen = strlen(p);

		if (suffixlen < stringlen) {
			off = stringlen - suffixlen;
			if (strcmp(p + off, *argv) == 0)
				p[off] = '\0';
		}
	}
	(void)printf("%s\n", p);
	exit(0);
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: basename string [suffix]\n");
	exit(1);
}
