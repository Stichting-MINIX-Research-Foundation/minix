/*	$NetBSD: expand.c,v 1.13 2009/04/12 02:51:36 lukem Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)expand.c	8.1 (Berkeley) 6/9/93";
#endif
__RCSID("$NetBSD: expand.c,v 1.13 2009/04/12 02:51:36 lukem Exp $");
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <err.h>

/*
 * expand - expand tabs to equivalent spaces
 */
size_t	nstops;
size_t	tabstops[100];

static	void	getstops(const char *);
	int	main(int, char **);
static	void	usage(void) __dead;

int
main(int argc, char *argv[])
{
	int c;
	size_t n, column;

	setprogname(argv[0]);

	/* handle obsolete syntax */
	while (argc > 1 &&
	    argv[1][0] == '-' && isdigit((unsigned char)argv[1][1])) {
		getstops(&argv[1][1]);
		argc--; argv++;
	}

	while ((c = getopt (argc, argv, "t:")) != -1) {
		switch (c) {
		case 't':
			getstops(optarg);
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	do {
		if (argc > 0) {
			if (freopen(argv[0], "r", stdin) == NULL)
				err(EXIT_FAILURE, "Cannot open `%s'", argv[0]);
			argc--, argv++;
		}
		column = 0;
		while ((c = getchar()) != EOF) {
			switch (c) {
			case '\t':
				if (nstops == 0) {
					do {
						putchar(' ');
						column++;
					} while (column & 07);
					continue;
				}
				if (nstops == 1) {
					do {
						putchar(' ');
						column++;
					} while (((column - 1) % tabstops[0])
					    != (tabstops[0] - 1));
					continue;
				}
				for (n = 0; n < nstops; n++)
					if (tabstops[n] > column)
						break;
				if (n == nstops) {
					putchar(' ');
					column++;
					continue;
				}
				while (column < tabstops[n]) {
					putchar(' ');
					column++;
				}
				continue;

			case '\b':
				if (column)
					column--;
				putchar('\b');
				continue;

			default:
				putchar(c);
				column++;
				continue;

			case '\n':
				putchar(c);
				column = 0;
				continue;
			}
		}
	} while (argc > 0);
	return EXIT_SUCCESS;
}

static void
getstops(const char *spec)
{
	int i;
	const char *cp = spec;

	nstops = 0;
	for (;;) {
		i = 0;
		while (*cp >= '0' && *cp <= '9')
			i = i * 10 + *cp++ - '0';
		if (i <= 0 || i > 256)
			errx(EXIT_FAILURE, "Too large tab stop spec `%d'", i);
		if (nstops > 0 && (size_t)i <= tabstops[nstops-1])
			errx(EXIT_FAILURE, "Out of order tabstop spec `%d'", i);
		if (nstops == sizeof(tabstops) / sizeof(tabstops[0]) - 1)
			errx(EXIT_FAILURE, "Too many tabstops");
		tabstops[nstops++] = i;
		if (*cp == '\0')
			break;
		if (*cp != ',' && *cp != ' ')
			errx(EXIT_FAILURE, "Illegal tab stop spec `%s'", spec);
		cp++;
	}
}

static void
usage(void)
{

	(void)fprintf(stderr, "Usage: %s [-t tablist] [file ...]\n",
	    getprogname());
	exit(EXIT_FAILURE);
}
