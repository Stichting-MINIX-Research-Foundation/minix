/*	$NetBSD: str.c,v 1.29 2013/08/11 01:54:35 dholland Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
static char sccsid[] = "@(#)str.c	8.2 (Berkeley) 4/28/95";
#endif
__RCSID("$NetBSD: str.c,v 1.29 2013/08/11 01:54:35 dholland Exp $");
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "extern.h"

struct str {
	enum { STRING1, STRING2 } which;
	enum { EOS, INFINITE, NORMAL, RANGE, SEQUENCE, SET } state;
	int cnt;			/* character count */
	int lastch;			/* last character */
	int equiv[2];			/* equivalence set */
	int *set;			/* set of characters */
	const char *str;		/* user's string */
};

static int backslash(STR *);
static int bracket(STR *);
static int c_class(const void *, const void *);
static int *genclass(const char *, size_t);
static void genequiv(STR *);
static int genrange(STR *);
static void genseq(STR *);

STR *
str_create(int whichstring, const char *txt)
{
	STR *s;

	s = malloc(sizeof(*s));
	if (s == NULL) {
		err(1, "Out of memory");
	}

	s->which = whichstring == 2 ? STRING2 : STRING1;
	s->state = NORMAL;
	s->cnt = 0;
	s->lastch = OOBCH;
	s->equiv[0] = 0;
	s->equiv[1] = OOBCH;
	s->set = NULL;
	s->str = txt;

	return s;
}

void
str_destroy(STR *s)
{
	if (s->set != NULL && s->set != s->equiv) {
		free(s->set);
	}
	free(s);
}

int
next(STR *s, int *ret)
{
	int ch;

	switch (s->state) {
	case EOS:
		*ret = s->lastch;
		return 0;
	case INFINITE:
		*ret = s->lastch;
		return 1;
	case NORMAL:
		ch = (unsigned char)s->str[0];
		switch (ch) {
		case '\0':
			s->state = EOS;
			*ret = s->lastch;
			return 0;
		case '\\':
			s->lastch = backslash(s);
			break;
		case '[':
			if (bracket(s)) {
				return next(s, ret);
			}
			/* FALLTHROUGH */
		default:
			++s->str;
			s->lastch = ch;
			break;
		}

		/* We can start a range at any time. */
		if (s->str[0] == '-' && genrange(s)) {
			return next(s, ret);
		}
		*ret = s->lastch;
		return 1;
	case RANGE:
		if (s->cnt == 0) {
			s->state = NORMAL;
			return next(s, ret);
		}
		s->cnt--;
		++s->lastch;
		*ret = s->lastch;
		return 1;
	case SEQUENCE:
		if (s->cnt == 0) {
			s->state = NORMAL;
			return next(s, ret);
		}
		s->cnt--;
		*ret = s->lastch;
		return 1;
	case SET:
		s->lastch = s->set[s->cnt++];
		if (s->lastch == OOBCH) {
			s->state = NORMAL;
			if (s->set != s->equiv) {
				free(s->set);
			}
			s->set = NULL;
			return next(s, ret);
		}
		*ret = s->lastch;
		return 1;
	}
	/* NOTREACHED */
	assert(0);
	*ret = s->lastch;
	return 0;
}

static int
bracket(STR *s)
{
	const char *p;
	int *q;

	switch (s->str[1]) {
	case ':':				/* "[:class:]" */
		if ((p = strstr(s->str + 2, ":]")) == NULL)
			return 0;
		s->str += 2;
		q = genclass(s->str, p - s->str);
		s->state = SET;
		s->set = q;
		s->cnt = 0;
		s->str = p + 2;
		return 1;
	case '=':				/* "[=equiv=]" */
		if ((p = strstr(s->str + 2, "=]")) == NULL)
			return 0;
		s->str += 2;
		genequiv(s);
		s->str = p + 2;
		return 1;
	default:				/* "[\###*n]" or "[#*n]" */
		if ((p = strpbrk(s->str + 2, "*]")) == NULL)
			return 0;
		if (p[0] != '*' || strchr(p, ']') == NULL)
			return 0;
		s->str += 1;
		genseq(s);
		return 1;
	}
	/* NOTREACHED */
}

typedef struct {
	const char *name;
	int (*func)(int);
} CLASS;

static const CLASS classes[] = {
	{ "alnum",  isalnum  },
	{ "alpha",  isalpha  },
	{ "blank",  isblank  },
	{ "cntrl",  iscntrl  },
	{ "digit",  isdigit  },
	{ "graph",  isgraph  },
	{ "lower",  islower  },
	{ "print",  isprint  },
	{ "punct",  ispunct  },
	{ "space",  isspace  },
	{ "upper",  isupper  },
	{ "xdigit", isxdigit },
};

typedef struct {
	const char *name;
	size_t len;
} CLASSKEY;

static int *
genclass(const char *class, size_t len)
{
	int ch;
	const CLASS *cp;
	CLASSKEY key;
	int *p;
	unsigned pos, num;

	/* Find the class */
	key.name = class;
	key.len = len;
	cp = bsearch(&key, classes, __arraycount(classes), sizeof(classes[0]),
		     c_class);
	if (cp == NULL) {
		errx(1, "unknown class %.*s", (int)len, class);
	}

	/*
	 * Figure out what characters are in the class
	 */

	num = NCHARS + 1;
	p = malloc(num * sizeof(*p));
	if (p == NULL) {
		err(1, "malloc");
	}

	pos = 0;
	for (ch = 0; ch < NCHARS; ch++) {
		if (cp->func(ch)) {
			p[pos++] = ch;
		}
	}

	p[pos++] = OOBCH;
	for (; pos < num; pos++) {
		p[pos] = 0;
	}

	return p;
}

static int
c_class(const void *av, const void *bv)
{
	const CLASSKEY *a = av;
	const CLASS *b = bv;
	size_t blen;
	int r;

	blen = strlen(b->name);
	r = strncmp(a->name, b->name, a->len);
	if (r != 0) {
		return r;
	}
	if (a->len < blen) {
		/* someone gave us a prefix of the right name */
		return -1;
	}
	assert(a-> len == blen);
	return 0;
}

/*
 * English doesn't have any equivalence classes, so for now
 * we just syntax check and grab the character.
 */
static void
genequiv(STR *s)
{
	int ch;

	ch = (unsigned char)s->str[0];
	if (ch == '\\') {
		s->equiv[0] = backslash(s);
	} else {
		s->equiv[0] = ch;
		s->str++;
	}
	if (s->str[0] != '=') {
		errx(1, "Misplaced equivalence equals sign");
	}
	s->str++;
	if (s->str[0] != ']') {
		errx(1, "Misplaced equivalence right bracket");
	}
	s->str++;

	s->cnt = 0;
	s->state = SET;
	s->set = s->equiv;
}

static int
genrange(STR *s)
{
	int stopval;
	const char *savestart;

	savestart = s->str++;
	stopval = s->str[0] == '\\' ? backslash(s) : (unsigned char)*s->str++;
	if (stopval < (unsigned char)s->lastch) {
		s->str = savestart;
		return 0;
	}
	s->cnt = stopval - s->lastch + 1;
	s->state = RANGE;
	--s->lastch;
	return 1;
}

static void
genseq(STR *s)
{
	char *ep;

	if (s->which == STRING1) {
		errx(1, "Sequences only valid in string2");
	}

	if (*s->str == '\\') {
		s->lastch = backslash(s);
	} else {
		s->lastch = (unsigned char)*s->str++;
	}
	if (*s->str != '*') {
		errx(1, "Misplaced sequence asterisk");
	}

	s->str++;
	switch (s->str[0]) {
	case '\\':
		s->cnt = backslash(s);
		break;
	case ']':
		s->cnt = 0;
		++s->str;
		break;
	default:
		if (isdigit((unsigned char)s->str[0])) {
			s->cnt = strtol(s->str, &ep, 0);
			if (*ep == ']') {
				s->str = ep + 1;
				break;
			}
		}
		errx(1, "illegal sequence count");
		/* NOTREACHED */
	}

	s->state = s->cnt ? SEQUENCE : INFINITE;
}

/*
 * Translate \??? into a character.  Up to 3 octal digits, if no digits either
 * an escape code or a literal character.
 */
static int
backslash(STR *s)
{
	int ch, cnt, val;

	cnt = val = 0;
	for (;;) {
		/* Consume the character we're already on. */
		s->str++;

		/* Look at the next character. */
		ch = (unsigned char)s->str[0];
		if (!isascii(ch) || !isdigit(ch)) {
			break;
		}
		val = val * 8 + ch - '0';
		if (++cnt == 3) {
			/* Enough digits; consume this one and stop */
			++s->str;
			break;
		}
	}
	if (cnt) {
		/* We saw digits, so return their value */
		return val;
	}
	if (ch == '\0') {
		/* \<end> -> \ */
		s->state = EOS;
		return '\\';
	}

	/* Consume the escaped character */
	s->str++;

	switch (ch) {
	case 'a':			/* escape characters */
		return '\7';
	case 'b':
		return '\b';
	case 'e':
		return '\033';
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case 'v':
		return '\13';
	default:			/* \q -> q */
		return ch;
	}
}
