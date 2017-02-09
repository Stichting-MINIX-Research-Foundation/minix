/*	$NetBSD: pmatch.c,v 1.6 2009/03/14 21:04:24 dsl Exp $	*/

/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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

#include <sys/param.h>
#include <lib/libkern/libkern.h>
/*
 * pmatch():
 *	Return 2 on exact match.
 *	Return 1 on substring match.
 *	Return 0 on no match.
 *	Return -1 on error.
 * *estr will point to the end of thelongest exact or substring match.
 */
int
pmatch(const char *string, const char *pattern, const char **estr)
{
	u_char stringc, patternc, rangec;
	int match, negate_range;
	const char *oestr, *pestr, *testr;

	if (estr == NULL)
		estr = &testr;

	for (;; ++string) {
		stringc = *string;
		switch (patternc = *pattern++) {
		case 0:
			*estr = string;
			return stringc == '\0' ? 2 : 1;
		case '?':
			if (stringc == '\0')
				return 0;
			*estr = string;
			break;
		case '*':
			if (!*pattern) {
				while (*string)
					string++;
				*estr = string;
				return 2;
			}
			oestr = *estr;
			pestr = NULL;

			do {
				switch (pmatch(string, pattern, estr)) {
				case -1:
					return -1;
				case 0:
					break;
				case 1:
					pestr = *estr;
					break;
				case 2:
					return 2;
				default:
					return -1;
				}
				*estr = string;
			}
			while (*string++);

			if (pestr) {
				*estr = pestr;
				return 1;
			} else {
				*estr = oestr;
				return 0;
			}

		case '[':
			match = 0;
			if ((negate_range = (*pattern == '^')) != 0)
				pattern++;
			while ((rangec = *pattern++) != '\0') {
				if (rangec == ']')
					break;
				if (match)
					continue;
				if (rangec == '-' && *(pattern - 2) != '[' &&
				    *pattern != ']') {
					match =
					    stringc <= (u_char)*pattern &&
					    (u_char)*(pattern - 2) <= stringc;
					pattern++;
				} else
					match = (stringc == rangec);
			}
			if (rangec == 0)
				return -1;
			if (match == negate_range)
				return 0;
			*estr = string;
			break;
		default:
			if (patternc != stringc)
				return 0;
			*estr = string;
			break;
		}
	}
}
