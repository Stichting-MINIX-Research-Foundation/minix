/*	$NetBSD: paste.c,v 1.16 2011/09/06 18:24:43 joerg Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)paste.c	8.1 (Berkeley) 6/6/93";*/
__RCSID("$NetBSD: paste.c,v 1.16 2011/09/06 18:24:43 joerg Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	parallel(int, char **);
static void	sequential(char **);
static int	tr(char *);
__dead static void	usage(void);

static char dflt_delim[] = "\t";
static char *delim = dflt_delim;
static int delimcnt = 1;

int
main(int argc, char **argv)
{
	int ch, seq;

	seq = 0;
	while ((ch = getopt(argc, argv, "d:s")) != -1) {
		switch (ch) {
		case 'd':
			delim = strdup(optarg);
			delimcnt = tr(delim);
			break;
		case 's':
			seq = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (seq)
		sequential(argv);
	else
		parallel(argc, argv);
	exit(0);
}

static void
parallel(int argc, char **argv)
{
	char ch, *dp, *line;
	FILE **fpp, *fp;
	size_t line_len;
	int cnt, output;

	fpp = calloc(argc, sizeof *fpp);
	if (fpp == NULL)
		err(1, "calloc");

	for (cnt = 0; cnt < argc; cnt++) {
		if (strcmp(argv[cnt], "-") == 0)
			fpp[cnt] = stdin;
		else if (!(fpp[cnt] = fopen(argv[cnt], "r")))
			err(1, "%s", argv[cnt]);
	}

	for (;;) {
		/* Start with the NUL at the end of 'delim' ... */
		dp = delim + delimcnt;
		output = 0;
		for (cnt = 0; cnt < argc; cnt++) {
			fp = fpp[cnt];
			if (fp == NULL)
				continue;
			line = fgetln(fp, &line_len);
			if (line == NULL) {
				/* Assume EOF */
				if (fp != stdin)
					fclose(fp);
				fpp[cnt] = NULL;
				continue;
			}
			/* Output enough separators to catch up */
			do {
				ch = *dp++;
				if (ch)
					putchar(ch);
				if (dp >= delim + delimcnt)
					dp = delim;
			} while (++output <= cnt);
			/* Remove any trailing newline - check for last line */
			if (line[line_len - 1] == '\n')
				line_len--;
			printf("%.*s", (int)line_len, line);
		}

		if (!output)
			break;

		/* Add separators to end of line */
		while (++output <= cnt) {
			ch = *dp++;
			if (ch)
				putchar(ch);
			if (dp >= delim + delimcnt)
				dp = delim;
		}
		putchar('\n');
	}

	free(fpp);
}

static void
sequential(char **argv)
{
	FILE *fp;
	int cnt;
	char ch, *p, *dp;
	char buf[_POSIX2_LINE_MAX + 1];

	for (; (p = *argv) != NULL; ++argv) {
		if (p[0] == '-' && !p[1])
			fp = stdin;
		else if (!(fp = fopen(p, "r"))) {
			warn("%s", p);
			continue;
		}
		if (fgets(buf, sizeof(buf), fp)) {
			for (cnt = 0, dp = delim;;) {
				if (!(p = strchr(buf, '\n')))
					err(1, "%s: input line too long.",
					    *argv);
				*p = '\0';
				(void)printf("%s", buf);
				if (!fgets(buf, sizeof(buf), fp))
					break;
				if ((ch = *dp++) != 0)
					putchar(ch);
				if (++cnt == delimcnt) {
					dp = delim;
					cnt = 0;
				}
			}
			putchar('\n');
		}
		if (fp != stdin)
			(void)fclose(fp);
	}
}

static int
tr(char *arg)
{
	int cnt;
	char ch, *p;

	for (p = arg, cnt = 0; (ch = *p++); ++arg, ++cnt)
		if (ch == '\\')
			switch(ch = *p++) {
			case 'n':
				*arg = '\n';
				break;
			case 't':
				*arg = '\t';
				break;
			case '0':
				*arg = '\0';
				break;
			default:
				*arg = ch;
				break;
		} else
			*arg = ch;

	if (!cnt)
		errx(1, "no delimiters specified.");
	*arg = '\0';
	return(cnt);
}

static void
usage(void)
{
	(void)fprintf(stderr, "paste: [-s] [-d delimiters] file ...\n");
	exit(1);
}
