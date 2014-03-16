/*	$NetBSD: morse.c,v 1.17 2012/06/19 05:46:08 dholland Exp $	*/

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
static char sccsid[] = "@(#)morse.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: morse.c,v 1.17 2012/06/19 05:46:08 dholland Exp $");
#endif
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char
	*const digit[] = {
	"-----",
	".----",
	"..---",
	"...--",
	"....-",
	".....",
	"-....",
	"--...",
	"---..",
	"----.",
},
	*const alph[] = {
	".-",
	"-...",
	"-.-.",
	"-..",
	".",
	"..-.",
	"--.",
	"....",
	"..",
	".---",
	"-.-",
	".-..",
	"--",
	"-.",
	"---",
	".--.",
	"--.-",
	".-.",
	"...",
	"-",
	"..-",
	"...-",
	".--",
	"-..-",
	"-.--",
	"--..",
};

static const struct punc {
	char c;
	const char *morse;
} other[] = {
	{ '.', ".-.-.-" },
	{ ',', "--..--" },
	{ ':', "---..." },
	{ '?', "..--.." },
	{ '\'', ".----." },
	{ '-', "-....-" },
	{ '/', "-..-." },
	{ '(', "-.--." },
	{ ')', "-.--.-" },
	{ '"', ".-..-." },
	{ '=', "-...-" },
	{ '+', ".-.-." },
	{ '\0', NULL }
};

int	main(int, char *[]);
static void morse(int);
static void decode(const char *);
static void show(const char *);

static int sflag;
static int dflag;

int
main(int argc, char **argv)
{
	int ch;
	char *p;

	/* Revoke setgid privileges */
	setgid(getgid());

	while ((ch = getopt(argc, argv, "ds")) != -1)
		switch((char)ch) {
		case 'd':
			dflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: morse [-ds] [string ...]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (dflag) {
		if (*argv) {
			do {
				decode(*argv);
			} while (*++argv);
		} else {
			char foo[10];	/* All morse chars shorter than this */
			int is_blank, i;

			i = 0;
			is_blank = 0;
			while ((ch = getchar()) != EOF) {
				if (ch == '-' || ch == '.') {
					foo[i++] = ch;
					if (i == 10) {
						/* overrun means gibberish--print 'x' and
						 * advance */
						i = 0;
						putchar('x');
						while ((ch = getchar()) != EOF &&
						    (ch == '.' || ch == '-'))
							;
						is_blank = 1;
					}
				} else if (i) {
					foo[i] = '\0';
					decode(foo);
					i = 0;
					is_blank = 0;
				} else if (isspace(ch)) {
					if (is_blank) {
						/* print whitespace for each double blank */
						putchar(' ');
						is_blank = 0;
					} else
						is_blank = 1;
				}
			}
		}
		putchar('\n');
	} else {
		if (*argv)
			do {
				for (p = *argv; *p; ++p)
					morse((int)*p);
				show("");
			} while (*++argv);
		else while ((ch = getchar()) != EOF)
			morse(ch);
		show("...-.-");	/* SK */
	}
	
	return 0;
}

void
decode(const char *s)
{
	int i;
	
	for (i = 0; i < 10; i++)
		if (strcmp(digit[i], s) == 0) {
			putchar('0' + i);
			return;
		}
	
	for (i = 0; i < 26; i++)
		if (strcmp(alph[i], s) == 0) {
			putchar('A' + i);
			return;
		}
	i = 0;
	while (other[i].c) {
		if (strcmp(other[i].morse, s) == 0) {
			putchar(other[i].c);
			return;
		}
		i++;
	}
	if (strcmp("...-.-", s) == 0)
		return;
	putchar('x');	/* line noise */
}

void
morse(int c)
{
	int i;

	if (isalpha(c))
		show(alph[c - (isupper(c) ? 'A' : 'a')]);
	else if (isdigit(c))
		show(digit[c - '0']);
	else if (isspace(c))
		show("");  /* could show BT for a pause */
	else {
		i = 0;
		while (other[i].c) {
			if (other[i].c == c) {
				show(other[i].morse);
				break;
			}
			i++;
		}
	}
}

void
show(const char *s)
{
	if (sflag)
		printf(" %s", s);
	else for (; *s; ++s)
		printf(" %s", *s == '.' ? "dit" : "daw");
	printf("\n");
}
