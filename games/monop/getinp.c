/*	$NetBSD: getinp.c,v 1.19 2012/06/19 05:35:32 dholland Exp $	*/

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
#if 0
static char sccsid[] = "@(#)getinp.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: getinp.c,v 1.19 2012/06/19 05:35:32 dholland Exp $");
#endif
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "monop.h"

#define	LINE	70

static char	buf[257];

static int comp(const char *);

int
getinp(const char *prompt, const char *const lst [])
{
	int i, n_match, match = 0;
	char *sp;

	for (;;) {
		printf("%s", prompt);
		fgets(buf, sizeof(buf), stdin);
		if (feof(stdin)) {
			return 0;
		}
		if (buf[0] == '?' && buf[1] == '\n') {
			printf("Valid inputs are: ");
			for (i = 0, match = 18; lst[i]; i++) {
				if ((match+=(n_match=strlen(lst[i]))) > LINE) {
					printf("\n\t");
					match = n_match + 8;
				}
				if (*lst[i] == '\0') {
					match += 8;
					printf("<RETURN>");
				}
				else
					printf("%s", lst[i]);
				if (lst[i+1])
					printf(", ");
				else
					putchar('\n');
				match += 2;
			}
			continue;
		}
		if ((sp = strchr(buf, '\n')) != NULL)
			*sp = '\0';
		for (sp = buf; *sp; sp++)
			*sp = tolower((unsigned char)*sp);
		for (i = n_match = 0; lst[i]; i++)
			if (comp(lst[i])) {
				n_match++;
				match = i;
			}
		if (n_match == 1)
			return match;
		else if (buf[0] != '\0')
			printf("Illegal response: \"%s\".  "
			    "Use '?' to get list of valid answers\n", buf);
	}
}

static int
comp(const char *s1)
{
	const char *sp, *tsp;
	char c;

	if (buf[0] != '\0')
		for (sp = buf, tsp = s1; *sp; ) {
			c = tolower((unsigned char)*tsp);
			tsp++;
			if (c != *sp++)
				return 0;
		}
	else if (*s1 != '\0')
		return 0;
	return 1;
}
