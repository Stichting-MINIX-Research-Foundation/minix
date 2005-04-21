/*
 * Regular expression matching for expr(1).  Bugs:  The upper bound of
 * a range specified by the \{ feature cannot be zero.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#include "bltin.h"


#define RE_END 0		/* end of regular expression */
#define RE_LITERAL 1		/* normal character follows */
#define RE_DOT 2		/* "." */
#define RE_CCL 3		/* "[...]" */
#define RE_NCCL 4		/* "[^...]" */
#define RE_LP 5			/* "\(" */
#define RE_RP 6			/* "\)" */
#define RE_MATCHED 7		/* "\digit" */
#define RE_EOS 8		/* "$" matches end of string */
#define RE_STAR 9		/* "*" */
#define RE_RANGE 10		/* "\{num,num\}" */



char *match_begin[10];
short match_length[10];
short number_parens;
static int match();



char *
re_compile(pattern)
	char *pattern;
	{
	register char *p;
	register char c;
	char *comp;
	register char *q;
	char *begin;
	char *endp;
	register int len;
	int first;
	int type;
	char *stackp;
	char stack[10];
	int paren_num;
	int i;
	char *malloc();

	p = pattern;
	if (*p == '^')
		p++;
	comp = q = malloc(2 * strlen(p) + 1);
	begin = q;
	stackp = stack;
	paren_num = 0;
	for (;;) {
		switch (c = *p++) {
		case '\0':
			*q = '\0';
			goto out;
		case '.':
			*q++ = RE_DOT;
			len = 1;
			break;
		case '[':
			begin = q;
			*q = RE_CCL;
			if (*p == '^') {
				*q = RE_NCCL;
				p++;
			}
			q++;
			first = 1;
			while (*p != ']' || first == 1) {
				if (p[1] == '-' && p[2] != ']') {
					*q++ = '-';
					*q++ = p[0];
					*q++ = p[2];
					p += 3;
				} else if (*p == '-') {
					*q++ = '-';
					*q++ = '-';
					*q++ = '-';
					p++;
				} else {
					*q++ = *p++;
				}
				first = 0;
			}
			p++;
			*q++ = '\0';
			len = q - begin;
			break;
		case '$':
			if (*p != '\0')
				goto dft;
			*q++ = RE_EOS;
			break;
		case '*':
			if (len == 0)
				goto dft;
			type = RE_STAR;
range:
			i = (type == RE_RANGE)? 3 : 1;
			endp = q + i;
			begin = q - len;
			do {
				--q;
				*(q + i) = *q;
			} while (--len > 0);
			q = begin;
			*q++ = type;
			if (type == RE_RANGE) {
				i = 0;
				while ((unsigned)(*p - '0') <= 9)
					i = 10 * i + (*p++ - '0');
				*q++ = i;
				if (*p != ',') {
					*q++ = i;
				} else {
					p++;
					i = 0;
					while ((unsigned)(*p - '0') <= 9)
						i = 10 * i + (*p++ - '0');
					*q++ = i;
				}
				if (*p != '\\' || *++p != '}')
					error("RE error");
				p++;
			}
			q = endp;
			break;
		case '\\':
			if ((c = *p++) == '(') {
				if (++paren_num > 9)
					error("RE error");
				*q++ = RE_LP;
				*q++ = paren_num;
				*stackp++ = paren_num;
				len = 0;
			} else if (c == ')') {
				if (stackp == stack)
					error("RE error");
				*q++ = RE_RP;
				*q++ = *--stackp;
				len = 0;
			} else if (c == '{') {
				type = RE_RANGE;
				goto range;
			} else if ((unsigned)(c - '1') < 9) {
				/* should check validity here */
				*q++ = RE_MATCHED;
				*q++ = c - '0';
				len = 2;
			} else {
				goto dft;
			}
			break;
		default:
dft:			*q++ = RE_LITERAL;
			*q++ = c;
			len = 2;
			break;
		}
	}
out:
	if (stackp != stack)
		error("RE error");
	number_parens = paren_num;
	return comp;
}



re_match(pattern, string)
	char *pattern;
	char *string;
	{
	char **pp;

	match_begin[0] = string;
	for (pp = &match_begin[1] ; pp <= &match_begin[9] ; pp++)
		*pp = 0;
	return match(pattern, string);
}



static
match(pattern, string)
	char *pattern;
	char *string;
	{
	register char *p, *q;
	int counting;
	int low, high, count;
	char *curpat;
	char *start_count;
	int negate;
	int found;
	char *r;
	int len;
	char c;

	p = pattern;
	q = string;
	counting = 0;
	for (;;) {
		if (counting) {
			if (++count > high)
				goto bad;
			p = curpat;
		}
		switch (*p++) {
		case RE_END:
			match_length[0] = q - match_begin[0];
			return 1;
		case RE_LITERAL:
			if (*q++ != *p++)
				goto bad;
			break;
		case RE_DOT:
			if (*q++ == '\0')
				goto bad;
			break;
		case RE_CCL:
			negate = 0;
			goto ccl;
		case RE_NCCL:
			negate = 1;
ccl:
			found = 0;
			c = *q++;
			while (*p) {
				if (*p == '-') {
					if (c >= *++p && c <= *++p)
						found = 1;
				} else {
					if (c == *p)
						found = 1;
				}
				p++;
			}
			p++;
			if (found == negate)
				goto bad;
			break;
		case RE_LP:
			match_begin[*p++] = q;
			break;
		case RE_RP:
			match_length[*p] = q - match_begin[*p];
			p++;
			break;
		case RE_MATCHED:
			r = match_begin[*p];
			len = match_length[*p++];
			while (--len >= 0) {
				if (*q++ != *r++)
					goto bad;
			}
			break;
		case RE_EOS:
			if (*q != '\0')
				goto bad;
			break;
		case RE_STAR:
			low = 0;
			high = 32767;
			goto range;
		case RE_RANGE:
			low = *p++;
			high = *p++;
			if (high == 0)
				high = 32767;
range:
			curpat = p;
			start_count = q;
			count = 0;
			counting++;
			break;
		}
	}
bad:
	if (! counting)
		return 0;
	len = 1;
	if (*curpat == RE_MATCHED)
		len = match_length[curpat[1]];
	while (--count >= low) {
		if (match(p, start_count + count * len))
			return 1;
	}
	return 0;
}
