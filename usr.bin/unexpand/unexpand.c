/*	$NetBSD: unexpand.c,v 1.14 2008/12/21 02:33:13 christos Exp $	*/

/*-
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
static char sccsid[] = "@(#)unexpand.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: unexpand.c,v 1.14 2008/12/21 02:33:13 christos Exp $");
#endif /* not lint */

/*
 * unexpand - put tabs into a file replacing blanks
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <util.h>


#define DSTOP	8
static int	all;
static size_t	nstops;
static size_t	maxstops;
static size_t	*tabstops;

static void	tabify(const char *, size_t);
static void	usage(void) __attribute__((__noreturn__));

static void
usage(void)
{
    (void)fprintf(stderr, "Usage: %s [-a] [-t tabstop] [file ...]\n",
	getprogname());
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	int c;
	char *ep, *tab;
	char *line;
	size_t len;
	unsigned long i;

	setprogname(argv[0]);

	while ((c = getopt(argc, argv, "at:")) != -1) {
		switch (c) {
		case 'a':
			if (nstops)
				usage();
			all++;
			break;
		case 't':
			if (all)
				usage();
			while ((tab = strsep(&optarg, ", \t")) != NULL) {
				if (*tab == '\0')
					continue;
				errno = 0;
				i = strtoul(tab, &ep, 0);
				if (*ep || (errno == ERANGE && i == ULONG_MAX))
					errx(EXIT_FAILURE,
					    "Invalid tabstop `%s'", tab);
				if (nstops >= maxstops) {
					maxstops += 20;
					tabstops = erealloc(tabstops, maxstops);
				}
				if (nstops && tabstops[nstops - 1] >= (size_t)i)
					errx(EXIT_FAILURE,
					    "Bad tabstop spec `%s', must be "
					    "greater than the previous `%zu'",
					    tab, tabstops[nstops - 1]);
				tabstops[nstops++] = i;
			}
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	for (i = 0; i < nstops; i++)
		fprintf(stderr, "%lu %zu\n", i, tabstops[i]);

	do {
		if (argc > 0) {
			if (freopen(argv[0], "r", stdin) == NULL)
				err(EXIT_FAILURE, "Cannot open `%s'", argv[0]);
			argc--, argv++;
		}
		while ((line = fgetln(stdin, &len)) != NULL)
			tabify(line, len);
	} while (argc > 0);
	return EXIT_SUCCESS;
}

static void
tabify(const char *line, size_t len)
{
	const char *e, *p;
	size_t dcol, ocol, limit, n;

	dcol = ocol = 0;
	limit = nstops == 0 ? UINT_MAX : tabstops[nstops - 1] - 1;
	e = line + len;
	for (p = line; p < e; p++) {
		if (*p == ' ') {
			dcol++;
			continue;
		} else if (*p == '\t') {
			if (nstops == 0) {
				dcol = (1 + dcol / DSTOP) * DSTOP;
				continue;
			} else {
				for (n = 0; tabstops[n] - 1 < dcol &&
				    n < nstops; n++)
					continue;
				if (n < nstops - 1 && tabstops[n] - 1 < limit) {
					dcol = tabstops[n];
					continue;
				}
			}
		}

		/* Output our tabs */
		if (nstops == 0) {
			while (((ocol + DSTOP) / DSTOP) <= (dcol / DSTOP)) {
				if (dcol - ocol < 2)
					break;
				if (putchar('\t') == EOF)
					goto out;
				ocol = (1 + ocol / DSTOP) * DSTOP;
			}
		} else {
			for (n = 0; tabstops[n] <= ocol && n < nstops; n++)
				continue;
			while (tabstops[n] <= dcol && ocol < dcol &&
			    n < nstops && ocol < limit) {
				if (putchar('\t') == EOF)
					goto out;
				ocol = tabstops[n++];
			}
		}

		/* Output remaining spaces */
		while (ocol < dcol && ocol < limit) {
			if (putchar(' ') == EOF)
				goto out;
			ocol++;
		}

		/* Output our char */
		if (putchar(*p) == EOF)
			goto out;
		if (*p == '\b') {
			if (ocol > 0) {
				ocol--;
				dcol--;
			}
		} else {
			ocol++;
			dcol++;
		}

		/* Output remainder of line */
		if (!all || dcol >= limit) {
			for (p++; p < e; p++)
				if (putchar(*p) == EOF)
					goto out;
			return;
		}
	}
	return;
out:
	err(EXIT_FAILURE, "write failed");
}
