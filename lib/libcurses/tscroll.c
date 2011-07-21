/*	$NetBSD: tscroll.c,v 1.13 2007/01/21 13:25:36 jdc Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)tscroll.c	8.4 (Berkeley) 7/27/94";
#else
__RCSID("$NetBSD: tscroll.c,v 1.13 2007/01/21 13:25:36 jdc Exp $");
#endif
#endif				/* not lint */

#include <string.h>
#include <stdarg.h>
#include "curses.h"
#include "curses_private.h"

#define	MAXRETURNSIZE	64

char   *
__tscroll(const char *cap, int n1, int n2)
{
	return (__parse_cap(cap, n1, n2));
}

/*
 * Routines to parse capabilities.  Derived from tgoto.c in termcap(3)
 * library.  Cap is a string containing printf type escapes to allow
 * scrolling.  The following escapes are defined for substituting n:
 *
 *	%d	as in printf
 *	%2	like %2d
 *	%3	like %3d
 *	%.	gives %c hacking special case characters
 *	%+x	like %c but adding x first
 *
 *	The codes below affect the state but don't use up a value.
 *
 *	%>xy	if value > x add y
 *	%i	increments n
 *	%%	gives %
 *	%B	BCD (2 decimal digits encoded in one byte)
 *	%D	Delta Data (backwards bcd)
 *
 * all other characters are ``self-inserting''.
 *
 * XXX:
 *	%r	reverse order of two parameters
 * is also defined but we don't support it (yet).
 */
char	*
__parse_cap (char const *cap, ...)
{
	va_list	ap;
	static char result[MAXRETURNSIZE];
	int     c, n;
	char   *dp;
	int	have_input;

	va_start (ap, cap);
	n = 0;			/* XXX gcc -Wuninitialized */

	if (cap == NULL)
		goto err;
#ifdef DEBUG
	{
		int	i;
		
		__CTRACE(__CTRACE_MISC, "__parse_cap: cap = ");
		for (i = 0; i < strlen(cap); i++)
			__CTRACE(__CTRACE_MISC, "%s", unctrl(cap[i]));
		__CTRACE(__CTRACE_MISC, "\n");
	}
#endif
	have_input = 0;
	for (dp = result; (c = *cap++) != '\0';) {
		if (c != '%') {
			*dp++ = c;
			continue;
		}
		switch (c = *cap++) {
		case 'n':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%n, val = %d\n", n);
#endif
			}
			n ^= 0140;
			continue;
		case 'd':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%d, val = %d\n", n);
#endif
			}
			if (n < 10)
				goto one;
			if (n < 100)
				goto two;
			/* FALLTHROUGH */
		case '3':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%3, val = %d\n", n);
#endif
			}
			*dp++ = (n / 100) | '0';
			n %= 100;
			/* FALLTHROUGH */
		case '2':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%2, val = %d\n", n);
#endif
			}
	two:		*dp++ = n / 10 | '0';
	one:		*dp++ = n % 10 | '0';
			have_input = 0;
			continue;
		case '>':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%>, val = %d\n", n);
#endif
			}
			if (n > *cap++)
				n += *cap++;
			else
				cap++;
			continue;
		case '+':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%+, val = %d\n", n);
#endif
			}
			n += *cap++;
			/* FALLTHROUGH */
		case '.':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%., val = %d\n", n);
#endif
			}
			*dp++ = n;
			have_input = 0;
			continue;
		case 'i':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%i, val = %d\n", n);
#endif
			}
			n++;
			continue;
		case '%':
			*dp++ = c;
			continue;
		case 'B':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%B, val = %d\n", n);
#endif
			}
			n = (n / 10 << 4) + n % 10;
			continue;
		case 'D':
			if (!have_input) {
				n = va_arg (ap, int);
				have_input = 1;
#ifdef DEBUG
				__CTRACE(__CTRACE_MISC,
				    "__parse_cap: %%D, val = %d\n", n);
#endif
			}
			n = n - 2 * (n % 16);
			continue;
			/*
			 * XXX
			 * System V terminfo files have lots of extra gunk.
			 * The only other one we've seen in capability strings
			 * is %pN, and it seems to work okay if we ignore it.
			 */
		case 'p':
			++cap;
			continue;
		default:
			goto err;
		}
	}
	*dp = '\0';
	va_end (ap);
	return (result);

err:	va_end (ap);
	return ((char *) "\0");
}
