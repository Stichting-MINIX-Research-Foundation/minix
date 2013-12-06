/* $NetBSD: tparm.c,v 1.15 2013/06/07 13:16:18 roy Exp $ */

/*
 * Copyright (c) 2009, 2011, 2013 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: tparm.c,v 1.15 2013/06/07 13:16:18 roy Exp $");
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term_private.h>
#include <term.h>

#define LONG_STR_MAX ((CHAR_BIT * sizeof(long)) / 3)
#define BUFINC 128	/* Size to increament the terminal buffer by */

#define VA_LONG_LONG	1
#define VA_CHAR_INT	2
//#define VA_CHAR_LONG	3	/* No need for this yet */

static TERMINAL *dumbterm; /* For non thread safe functions */

typedef struct {
	long nums[20];
	char *strings[20];
	size_t offset;
} TPSTACK;

typedef struct {
	long num;
	char *string;
} TPVAR;

static int
push(long num, char *string, TPSTACK *stack)
{
	if (stack->offset >= sizeof(stack->nums)) {
		errno = E2BIG;
		return -1;
	}
	stack->nums[stack->offset] = num;
	stack->strings[stack->offset] = string;
	stack->offset++;
	return 0;
}

static int
pop(long *num, char **string, TPSTACK *stack)
{
	if (stack->offset == 0) {
		if (num)
			*num = 0;
		if (string)
			*string = NULL;
		errno = E2BIG;
		return -1;
	}
	stack->offset--;
	if (num)
		*num = stack->nums[stack->offset];
	if (string)
		*string = stack->strings[stack->offset];
	return 0;
}

static char *
checkbuf(TERMINAL *term, size_t len)
{
	char *buf;

	if (term->_bufpos + len >= term->_buflen) {
		len = term->_buflen + MAX(len, BUFINC);
		buf = realloc(term->_buf, len);
		if (buf == NULL)
			return NULL;
		term->_buf = buf;
		term->_buflen = len;
	}
	return term->_buf;
}

static size_t
ochar(TERMINAL *term, int c)
{
	if (c == 0)
		c = 0200;
	/* Check we have space and a terminator */
	if (checkbuf(term, 2) == NULL)
		return 0;
	term->_buf[term->_bufpos++] = (char)c;
	return 1;
}

static size_t
onum(TERMINAL *term, const char *fmt, int num, unsigned int len)
{
	size_t l;

	if (len < LONG_STR_MAX)
		len = LONG_STR_MAX;
	if (checkbuf(term, len + 2) == NULL)
		return 0;
	l = sprintf(term->_buf + term->_bufpos, fmt, num);
	term->_bufpos += l;
	return l;
}

/*
  Make a pass through the string so we can work out
  which parameters are ints and which are char *.
  Basically we only use char * if %p[1-9] is followed by %l or %s.
*/
int
_ti_parm_analyse(const char *str, int *piss, int piss_len)
{
	int nparm, lpop;
	char c;

	nparm = 0;
	lpop = -1;
	while ((c = *str++) != '\0') {
		if (c != '%')
			continue;
		c = *str++;
		switch (c) {
			case 'l': /* FALLTHROUGH */
			case 's':
				if (lpop > 0) {
					if (lpop <= piss_len)
						piss[lpop - 1] = 1;
					else if (piss)
						errno = E2BIG;
				}
				break;
			case 'p':
				c = *str++;
				if (c < '1' || c > '9') {
					errno = EINVAL;
					continue;
				} else {
					lpop = c - '0';
					if (lpop > nparm)
						nparm = lpop;
				}
				break;
			default:
				lpop = -1;
		}
	}

	return nparm;
}

static char *
_ti_tiparm(TERMINAL *term, const char *str, int va_type, va_list parms)
{
	char c, fmt[64], *fp, *ostr;
	long val, val2;
	long dnums[26]; /* dynamic variables a-z, not preserved */
	size_t l, max;
	TPSTACK stack;
	TPVAR params[TPARM_MAX];
	unsigned int done, dot, minus, width, precision, olen;
	int piss[TPARM_MAX]; /* Parameter IS String - piss ;) */

	if (str == NULL)
		return NULL;

	/*
	  If not passed a terminal, malloc a dummy one.
	  This means we can preserve buffers and variables per terminal and
	  still work with non thread safe functions (which sadly are still the
	  norm and standard).
	*/
	if (term == NULL) {
		if (dumbterm == NULL) {
			dumbterm = malloc(sizeof(*dumbterm));
			if (dumbterm == NULL)
				return NULL;
			dumbterm->_buflen = 0;
		}
		term = dumbterm;
	}

	term->_bufpos = 0;
	/* Ensure we have an initial buffer */
	if (term->_buflen == 0) {
		term->_buf = malloc(BUFINC);
		if (term->_buf == NULL)
			return NULL;
		term->_buflen = BUFINC;
	}

	memset(&piss, 0, sizeof(piss));
	max = _ti_parm_analyse(str, piss, TPARM_MAX);

	/* Put our parameters into variables */
	memset(&params, 0, sizeof(params));
	for (l = 0; l < max; l++) {
		if (piss[l]) {
			if (va_type == VA_LONG_LONG) {
				/* This only works if char * fits into a long
				 * on this platform. */
				if (sizeof(char *) <= sizeof(long)/*CONSTCOND*/)
					params[l].string =
					    (char *)va_arg(parms, long);
				else {
					errno = ENOTSUP;
					return NULL;
				}
			} else
				params[l].string = va_arg(parms, char *);
		} else {
			if (va_type == VA_CHAR_INT)
				params[l].num = (long)va_arg(parms, int);
			else
				params[l].num = va_arg(parms, long);
		}
	}

	memset(&stack, 0, sizeof(stack));
	while ((c = *str++) != '\0') {
		if (c != '%' || (c = *str++) == '%') {
			if (c == '\0')
				break;
			if (ochar(term, c) == 0)
				return NULL;
			continue;
		}

		/* Handle formatting. */
		fp = fmt;
		*fp++ = '%';
		done = dot = minus = width = precision = 0;
		val = 0;
		while (done == 0 && (size_t)(fp - fmt) < sizeof(fmt)) {
			switch (c) {
			case 'c': /* FALLTHROUGH */
			case 's':
				*fp++ = c;
				done = 1;
				break;
			case 'd': /* FALLTHROUGH */
			case 'o': /* FALLTHROUGH */
			case 'x': /* FALLTHROUGH */
			case 'X': /* FALLTHROUGH */
				*fp++ = 'l';
				*fp++ = c;
				done = 1;
				break;
			case '#': /* FALLTHROUGH */
			case ' ':
				*fp++ = c;
				break;
			case '.':
				*fp++ = c;
				if (dot == 0) {
					dot = 1;
					width = val;
				} else
					done = 2;
				val = 0;
				break;
			case ':':
				minus = 1;
				break;
			case '-':
				if (minus)
					*fp++ = c;
				else
					done = 1;
				break;
			default:
				if (isdigit((unsigned char)c)) {
					val = (val * 10) + (c - '0');
					if (val > 10000)
						done = 2;
					else
						*fp++ = c;
				} else
					done = 1;
			}
			if (done == 0)
				c = *str++;
		}
		if (done == 2) {
			/* Found an error in the format */
			fp = fmt + 1;
			*fp = *str;
			olen = 0;
		} else {
			if (dot == 0)
				width = val;
			else
				precision = val;
			olen = MAX(width, precision);
		}
		*fp++ = '\0';

		/* Handle commands */
		switch (c) {
		case 'c':
			pop(&val, NULL, &stack);
			if (ochar(term, (unsigned char)val) == 0)
				return NULL;
			break;
		case 's':
			pop(NULL, &ostr, &stack);
			if (ostr != NULL) {
				l = strlen(ostr);
				if (l < (size_t)olen)
					l = olen;
				if (checkbuf(term, (size_t)(l + 1)) == NULL)
					return NULL;
				l = sprintf(term->_buf + term->_bufpos,
				    fmt, ostr);
				term->_bufpos += l;
			}
			break;
		case 'l':
			pop(NULL, &ostr, &stack);
			if (ostr == NULL)
				l = 0;
			else
				l = strlen(ostr);
#ifdef NCURSES_COMPAT_57
			if (onum(term, "%ld", (long)l, 0) == 0)
				return NULL;
#else
			push((long)l, NULL, &stack);
#endif
			break;
		case 'd': /* FALLTHROUGH */
		case 'o': /* FALLTHROUGH */
		case 'x': /* FALLTHROUGH */
		case 'X':
			pop(&val, NULL, &stack);
			if (onum(term, fmt, (int)val, olen) == 0)
				return NULL;
			break;
		case 'p':
			if (*str < '1' || *str > '9')
				break;
			l = *str++ - '1';
			if (push(params[l].num, params[l].string, &stack))
				return NULL;
			break;
		case 'P':
			pop(&val, NULL, &stack);
			if (*str >= 'a' && *str <= 'z')
				dnums[*str - 'a'] = val;
			else if (*str >= 'A' && *str <= 'Z')
				term->_snums[*str - 'A'] = val;
			break;
		case 'g':
			if (*str >= 'a' && *str <= 'z') {
				if (push(dnums[*str - 'a'], NULL, &stack))
					return NULL;
			} else if (*str >= 'A' && *str <= 'Z') {
				if (push(term->_snums[*str - 'A'],
					NULL, &stack))
					return NULL;
			}
			break;
		case 'i':
			if (piss[0] == 0)
				params[0].num++;
			if (piss[1] == 0)
				params[1].num++;
			break;
		case '\'':
			if (push((long)(unsigned char)*str++, NULL, &stack))
				return NULL;
			while (*str != '\0' && *str != '\'')
				str++;
			if (*str == '\'')
				str++;
			break;
		case '{':
			val = 0;
			for (; isdigit((unsigned char)*str);  str++)
				val = (val * 10) + (*str - '0');
			if (push(val, NULL, &stack))
				return NULL;
			while (*str != '\0' && *str != '}')
				str++;
			if (*str == '}')
				str++;
			break;
		case '+': /* FALLTHROUGH */
		case '-': /* FALLTHROUGH */
		case '*': /* FALLTHROUGH */
		case '/': /* FALLTHROUGH */
		case 'm': /* FALLTHROUGH */
		case 'A': /* FALLTHROUGH */
		case 'O': /* FALLTHROUGH */
		case '&': /* FALLTHROUGH */
		case '|': /* FALLTHROUGH */
		case '^': /* FALLTHROUGH */
		case '=': /* FALLTHROUGH */
		case '<': /* FALLTHROUGH */
		case '>':
			pop(&val, NULL, &stack);
			pop(&val2, NULL, &stack);
			switch (c) {
			case '+':
				val = val + val2;
				break;
			case '-':
				val = val2 - val;
				break;
			case '*':
				val = val * val2;
				break;
			case '/':
				val = val ? val2 / val : 0;
				break;
			case 'm':
				val = val ? val2 % val : 0;
				break;
			case 'A':
				val = val && val2;
				break;
			case 'O':
				val = val || val2;
				break;
			case '&':
				val = val & val2;
				break;
			case '|':
				val = val | val2;
				break;
			case '^':
				val = val ^ val2;
				break;
			case '=':
				val = val == val2;
				break;
			case '<':
				val = val2 < val;
				break;
			case '>':
				val = val2 > val;
				break;
			}
			if (push(val, NULL, &stack))
				return NULL;
			break;
		case '!':
		case '~':
			pop(&val, NULL, &stack);
			switch (c) {
			case '!':
				val = !val;
				break;
			case '~':
				val = ~val;
				break;
			}
			if (push(val, NULL, &stack))
				return NULL;
			break;
		case '?': /* if */
			break;
		case 't': /* then */
			pop(&val, NULL, &stack);
			if (val == 0) {
				l = 0;
				for (; *str != '\0'; str++) {
					if (*str != '%')
						continue;
					str++;
					if (*str == '?')
						l++;
					else if (*str == ';') {
						if (l > 0)
							l--;
						else {
							str++;
							break;
						}
					} else if (*str == 'e' && l == 0) {
						str++;
						break;
					}
				}
			}
			break;
		case 'e': /* else */
			l = 0;
			for (; *str != '\0'; str++) {
				if (*str != '%')
					continue;
				str++;
				if (*str == '?')
					l++;
				else if (*str == ';') {
					if (l > 0)
						l--;
					else {
						str++;
						break;
					}
				}
			}
			break;
		case ';': /* fi */
			break;
		}
	}
	term->_buf[term->_bufpos] = '\0';
	return term->_buf;
}

char *
ti_tiparm(TERMINAL *term, const char *str, ...)
{
	va_list va;
	char *ret;

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(str != NULL);

	va_start(va, str);
	ret = _ti_tiparm(term, str, VA_CHAR_INT, va);
	va_end(va);
	return ret;
}

char *
tiparm(const char *str, ...)
{
	va_list va;
	char *ret;

	_DIAGASSERT(str != NULL);

	va_start(va, str);
	ret = _ti_tiparm(NULL, str, VA_CHAR_INT, va);
	va_end(va);
	return ret;
}

#ifdef VA_CHAR_LONG
char *
ti_tlparm(TERMINAL *term, const char *str, ...)
{
	va_list va;
	char *ret;

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(str != NULL);

	va_start(va, str);
	ret = _ti_tiparm(term, str, VA_CHAR_LONG, va);
	va_end(va);
	return ret;
}

char *
tlparm(const char *str, ...)
{
	va_list va;
	char *ret;

	_DIAGASSERT(str != NULL);

	va_start(va, str);
	ret = _ti_tiparm(NULL, str, VA_CHAR_LONG, va);
	va_end(va);
	return ret;
}
#endif

static char *
_tparm(const char *str, ...)
{
	va_list va;
	char *ret;

	_DIAGASSERT(str != NULL);

	va_start(va, str);
	ret = _ti_tiparm(NULL, str, VA_LONG_LONG, va);
	va_end(va);
	return ret;
}

char *
tparm(const char *str,
    long p1, long p2, long p3, long p4, long p5,
    long p6, long p7, long p8, long p9)
{

	return _tparm(str, p1, p2, p3, p4, p5, p6, p7, p8, p9);
}
