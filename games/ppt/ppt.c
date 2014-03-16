/*	$NetBSD: ppt.c,v 1.19 2011/08/29 20:30:37 joerg Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ppt.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: ppt.c,v 1.19 2011/08/29 20:30:37 joerg Exp $");
#endif
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	EDGE	"___________"

static void putppt(int);
static int getppt(const char *);

__dead static void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-d] [string ...]\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	char *p, buf[132];
	int c, start, neednl, dflag;

	/* Revoke setgid privileges */
	setgid(getgid());

	dflag = 0;
	while ((c = getopt(argc, argv, "dh")) != -1)
		switch(c) {
		case 'd':
			dflag = 1;
			break;
		case 'h':
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (dflag) {
		if (argc > 0)
			usage();

		start = 0;
		neednl = 0;
		while (fgets(buf, sizeof(buf), stdin) != NULL) {
			c = getppt(buf);
			if (c < 0) {
				if (start) {
					/* lost sync? */
					if (neednl)
						putchar('\n');
					exit(0);
				} else
					continue;
			}
			start = 1;
			putchar(c);
			neednl = (c != '\n');
		}
		if (!feof(stdin))
			err(1, "fgets");
		if (neednl)
			putchar('\n');
	} else {
		(void) puts(EDGE);
		if (argc > 0)
			while ((p = *argv++)) {
				for (; *p; ++p)
					putppt((int)*p);
				if ((*(argv)))
					putppt((int)' ');
			}
		else while ((c = getchar()) != EOF)
			putppt(c);
		(void) puts(EDGE);
	}
	exit(0);
}

static void
putppt(int c)
{
	int i;

	(void) putchar('|');
	for (i = 7; i >= 0; i--) {
		if (i == 2)
			(void) putchar('.');	/* feed hole */
		if ((c&(1<<i)) != 0)
			(void) putchar('o');
		else
			(void) putchar(' ');
	}
	(void) putchar('|');
	(void) putchar('\n');
}

static int
getppt(const char *buf)
{
	const char *p = strchr(buf, '.');
	int c;

	if (p == NULL)
		return (-1);

	c = 0;
	if (p[ 3] != ' ')
		c |= 0001;
	if (p[ 2] != ' ')
		c |= 0002;
	if (p[ 1] != ' ')
		c |= 0004;
	if (p[-1] != ' ')
		c |= 0010;
	if (p[-2] != ' ')
		c |= 0020;
	if (p[-3] != ' ')
		c |= 0040;
	if (p[-4] != ' ')
		c |= 0100;
	if (p[-5] != ' ')
		c |= 0200;

	return (c);
}
