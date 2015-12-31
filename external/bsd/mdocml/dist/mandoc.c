/*	Id: mandoc.c,v 1.75 2013/12/31 23:23:10 schwarze Exp  */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2012, 2013 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"

#define DATESIZE 32

static	int	 a2time(time_t *, const char *, const char *);
static	char	*time2a(time_t);


enum mandoc_esc
mandoc_escape(const char **end, const char **start, int *sz)
{
	const char	*local_start;
	int		 local_sz;
	char		 term;
	enum mandoc_esc	 gly; 

	/*
	 * When the caller doesn't provide return storage,
	 * use local storage.
	 */

	if (NULL == start)
		start = &local_start;
	if (NULL == sz)
		sz = &local_sz;

	/*
	 * Beyond the backslash, at least one input character
	 * is part of the escape sequence.  With one exception
	 * (see below), that character won't be returned.
	 */

	gly = ESCAPE_ERROR;
	*start = ++*end;
	*sz = 0;
	term = '\0';

	switch ((*start)[-1]) {
	/*
	 * First the glyphs.  There are several different forms of
	 * these, but each eventually returns a substring of the glyph
	 * name.
	 */
	case ('('):
		gly = ESCAPE_SPECIAL;
		*sz = 2;
		break;
	case ('['):
		gly = ESCAPE_SPECIAL;
		/*
		 * Unicode escapes are defined in groff as \[uXXXX] to
		 * \[u10FFFF], where the contained value must be a valid
		 * Unicode codepoint.  Here, however, only check whether
		 * it's not a zero-width escape.
		 */
		if ('u' == (*start)[0] && ']' != (*start)[1])
			gly = ESCAPE_UNICODE;
		term = ']';
		break;
	case ('C'):
		if ('\'' != **start)
			return(ESCAPE_ERROR);
		*start = ++*end;
		if ('u' == (*start)[0] && '\'' != (*start)[1])
			gly = ESCAPE_UNICODE;
		else
			gly = ESCAPE_SPECIAL;
		term = '\'';
		break;

	/*
	 * Escapes taking no arguments at all.
	 */
	case ('d'):
		/* FALLTHROUGH */
	case ('u'):
		return(ESCAPE_IGNORE);

	/*
	 * The \z escape is supposed to output the following
	 * character without advancing the cursor position.  
	 * Since we are mostly dealing with terminal mode,
	 * let us just skip the next character.
	 */
	case ('z'):
		return(ESCAPE_SKIPCHAR);

	/*
	 * Handle all triggers matching \X(xy, \Xx, and \X[xxxx], where
	 * 'X' is the trigger.  These have opaque sub-strings.
	 */
	case ('F'):
		/* FALLTHROUGH */
	case ('g'):
		/* FALLTHROUGH */
	case ('k'):
		/* FALLTHROUGH */
	case ('M'):
		/* FALLTHROUGH */
	case ('m'):
		/* FALLTHROUGH */
	case ('n'):
		/* FALLTHROUGH */
	case ('V'):
		/* FALLTHROUGH */
	case ('Y'):
		gly = ESCAPE_IGNORE;
		/* FALLTHROUGH */
	case ('f'):
		if (ESCAPE_ERROR == gly)
			gly = ESCAPE_FONT;
		switch (**start) {
		case ('('):
			*start = ++*end;
			*sz = 2;
			break;
		case ('['):
			*start = ++*end;
			term = ']';
			break;
		default:
			*sz = 1;
			break;
		}
		break;

	/*
	 * These escapes are of the form \X'Y', where 'X' is the trigger
	 * and 'Y' is any string.  These have opaque sub-strings.
	 */
	case ('A'):
		/* FALLTHROUGH */
	case ('b'):
		/* FALLTHROUGH */
	case ('B'):
		/* FALLTHROUGH */
	case ('D'):
		/* FALLTHROUGH */
	case ('o'):
		/* FALLTHROUGH */
	case ('R'):
		/* FALLTHROUGH */
	case ('w'):
		/* FALLTHROUGH */
	case ('X'):
		/* FALLTHROUGH */
	case ('Z'):
		if ('\'' != **start)
			return(ESCAPE_ERROR);
		gly = ESCAPE_IGNORE;
		*start = ++*end;
		term = '\'';
		break;

	/*
	 * These escapes are of the form \X'N', where 'X' is the trigger
	 * and 'N' resolves to a numerical expression.
	 */
	case ('h'):
		/* FALLTHROUGH */
	case ('H'):
		/* FALLTHROUGH */
	case ('L'):
		/* FALLTHROUGH */
	case ('l'):
		/* FALLTHROUGH */
	case ('S'):
		/* FALLTHROUGH */
	case ('v'):
		/* FALLTHROUGH */
	case ('x'):
		if ('\'' != **start)
			return(ESCAPE_ERROR);
		gly = ESCAPE_IGNORE;
		*start = ++*end;
		term = '\'';
		break;

	/*
	 * Special handling for the numbered character escape.
	 * XXX Do any other escapes need similar handling?
	 */
	case ('N'):
		if ('\0' == **start)
			return(ESCAPE_ERROR);
		(*end)++;
		if (isdigit((unsigned char)**start)) {
			*sz = 1;
			return(ESCAPE_IGNORE);
		}
		(*start)++;
		while (isdigit((unsigned char)**end))
			(*end)++;
		*sz = *end - *start;
		if ('\0' != **end)
			(*end)++;
		return(ESCAPE_NUMBERED);

	/* 
	 * Sizes get a special category of their own.
	 */
	case ('s'):
		gly = ESCAPE_IGNORE;

		/* See +/- counts as a sign. */
		if ('+' == **end || '-' == **end || ASCII_HYPH == **end)
			(*end)++;

		switch (**end) {
		case ('('):
			*start = ++*end;
			*sz = 2;
			break;
		case ('['):
			*start = ++*end;
			term = ']';
			break;
		case ('\''):
			*start = ++*end;
			term = '\'';
			break;
		default:
			*sz = 1;
			break;
		}

		break;

	/*
	 * Anything else is assumed to be a glyph.
	 * In this case, pass back the character after the backslash.
	 */
	default:
		gly = ESCAPE_SPECIAL;
		*start = --*end;
		*sz = 1;
		break;
	}

	assert(ESCAPE_ERROR != gly);

	/*
	 * Read up to the terminating character,
	 * paying attention to nested escapes.
	 */

	if ('\0' != term) {
		while (**end != term) {
			switch (**end) {
			case ('\0'):
				return(ESCAPE_ERROR);
			case ('\\'):
				(*end)++;
				if (ESCAPE_ERROR ==
				    mandoc_escape(end, NULL, NULL))
					return(ESCAPE_ERROR);
				break;
			default:
				(*end)++;
				break;
			}
		}
		*sz = (*end)++ - *start;
	} else {
		assert(*sz > 0);
		if ((size_t)*sz > strlen(*start))
			return(ESCAPE_ERROR);
		*end += *sz;
	}

	/* Run post-processors. */

	switch (gly) {
	case (ESCAPE_FONT):
		if (2 == *sz) {
			if ('C' == **start) {
				/*
				 * Treat constant-width font modes
				 * just like regular font modes.
				 */
				(*start)++;
				(*sz)--;
			} else {
				if ('B' == (*start)[0] && 'I' == (*start)[1])
					gly = ESCAPE_FONTBI;
				break;
			}
		} else if (1 != *sz)
			break;

		switch (**start) {
		case ('3'):
			/* FALLTHROUGH */
		case ('B'):
			gly = ESCAPE_FONTBOLD;
			break;
		case ('2'):
			/* FALLTHROUGH */
		case ('I'):
			gly = ESCAPE_FONTITALIC;
			break;
		case ('P'):
			gly = ESCAPE_FONTPREV;
			break;
		case ('1'):
			/* FALLTHROUGH */
		case ('R'):
			gly = ESCAPE_FONTROMAN;
			break;
		}
		break;
	case (ESCAPE_SPECIAL):
		if (1 == *sz && 'c' == **start)
			gly = ESCAPE_NOSPACE;
		break;
	default:
		break;
	}

	return(gly);
}

void *
mandoc_calloc(size_t num, size_t size)
{
	void		*ptr;

	ptr = calloc(num, size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}


void *
mandoc_malloc(size_t size)
{
	void		*ptr;

	ptr = malloc(size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}


void *
mandoc_realloc(void *ptr, size_t size)
{

	ptr = realloc(ptr, size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}

char *
mandoc_strndup(const char *ptr, size_t sz)
{
	char		*p;

	p = mandoc_malloc(sz + 1);
	memcpy(p, ptr, sz);
	p[(int)sz] = '\0';
	return(p);
}

char *
mandoc_strdup(const char *ptr)
{
	char		*p;

	p = strdup(ptr);
	if (NULL == p) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(p);
}

/*
 * Parse a quoted or unquoted roff-style request or macro argument.
 * Return a pointer to the parsed argument, which is either the original
 * pointer or advanced by one byte in case the argument is quoted.
 * NUL-terminate the argument in place.
 * Collapse pairs of quotes inside quoted arguments.
 * Advance the argument pointer to the next argument,
 * or to the NUL byte terminating the argument line.
 */
char *
mandoc_getarg(struct mparse *parse, char **cpp, int ln, int *pos)
{
	char	 *start, *cp;
	int	  quoted, pairs, white;

	/* Quoting can only start with a new word. */
	start = *cpp;
	quoted = 0;
	if ('"' == *start) {
		quoted = 1;
		start++;
	} 

	pairs = 0;
	white = 0;
	for (cp = start; '\0' != *cp; cp++) {

		/*
		 * Move the following text left
		 * after quoted quotes and after "\\" and "\t".
		 */
		if (pairs)
			cp[-pairs] = cp[0];

		if ('\\' == cp[0]) {
			/*
			 * In copy mode, translate double to single
			 * backslashes and backslash-t to literal tabs.
			 */
			switch (cp[1]) {
			case ('t'):
				cp[0] = '\t';
				/* FALLTHROUGH */
			case ('\\'):
				pairs++;
				cp++;
				break;
			case (' '):
				/* Skip escaped blanks. */
				if (0 == quoted)
					cp++;
				break;
			default:
				break;
			}
		} else if (0 == quoted) {
			if (' ' == cp[0]) {
				/* Unescaped blanks end unquoted args. */
				white = 1;
				break;
			}
		} else if ('"' == cp[0]) {
			if ('"' == cp[1]) {
				/* Quoted quotes collapse. */
				pairs++;
				cp++;
			} else {
				/* Unquoted quotes end quoted args. */
				quoted = 2;
				break;
			}
		}
	}

	/* Quoted argument without a closing quote. */
	if (1 == quoted)
		mandoc_msg(MANDOCERR_BADQUOTE, parse, ln, *pos, NULL);

	/* NUL-terminate this argument and move to the next one. */
	if (pairs)
		cp[-pairs] = '\0';
	if ('\0' != *cp) {
		*cp++ = '\0';
		while (' ' == *cp)
			cp++;
	}
	*pos += (int)(cp - start) + (quoted ? 1 : 0);
	*cpp = cp;

	if ('\0' == *cp && (white || ' ' == cp[-1]))
		mandoc_msg(MANDOCERR_EOLNSPACE, parse, ln, *pos, NULL);

	return(start);
}

static int
a2time(time_t *t, const char *fmt, const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	pp = NULL;
#ifdef	HAVE_STRPTIME
	pp = strptime(p, fmt, &tm);
#endif
	if (NULL != pp && '\0' == *pp) {
		*t = mktime(&tm);
		return(1);
	}

	return(0);
}

static char *
time2a(time_t t)
{
	struct tm	*tm;
	char		*buf, *p;
	size_t		 ssz;
	int		 isz;

	tm = localtime(&t);

	/*
	 * Reserve space:
	 * up to 9 characters for the month (September) + blank
	 * up to 2 characters for the day + comma + blank
	 * 4 characters for the year and a terminating '\0'
	 */
	p = buf = mandoc_malloc(10 + 4 + 4 + 1);

	if (0 == (ssz = strftime(p, 10 + 1, "%B ", tm)))
		goto fail;
	p += (int)ssz;

	if (-1 == (isz = snprintf(p, 4 + 1, "%d, ", tm->tm_mday)))
		goto fail;
	p += isz;

	if (0 == strftime(p, 4 + 1, "%Y", tm))
		goto fail;
	return(buf);

fail:
	free(buf);
	return(NULL);
}

char *
mandoc_normdate(struct mparse *parse, char *in, int ln, int pos)
{
	char		*out;
	time_t		 t;

	if (NULL == in || '\0' == *in ||
	    0 == strcmp(in, "$" "Mdocdate$")) {
		mandoc_msg(MANDOCERR_NODATE, parse, ln, pos, NULL);
		time(&t);
	}
	else if (a2time(&t, "%Y-%m-%d", in))
		t = 0;
	else if (!a2time(&t, "$" "Mdocdate: %b %d %Y $", in) &&
	    !a2time(&t, "%b %d, %Y", in)) {
		mandoc_msg(MANDOCERR_BADDATE, parse, ln, pos, NULL);
		t = 0;
	}
	out = t ? time2a(t) : NULL;
	return(out ? out : mandoc_strdup(in));
}

int
mandoc_eos(const char *p, size_t sz)
{
	const char	*q;
	int		 enclosed, found;

	if (0 == sz)
		return(0);

	/*
	 * End-of-sentence recognition must include situations where
	 * some symbols, such as `)', allow prior EOS punctuation to
	 * propagate outward.
	 */

	enclosed = found = 0;
	for (q = p + (int)sz - 1; q >= p; q--) {
		switch (*q) {
		case ('\"'):
			/* FALLTHROUGH */
		case ('\''):
			/* FALLTHROUGH */
		case (']'):
			/* FALLTHROUGH */
		case (')'):
			if (0 == found)
				enclosed = 1;
			break;
		case ('.'):
			/* FALLTHROUGH */
		case ('!'):
			/* FALLTHROUGH */
		case ('?'):
			found = 1;
			break;
		default:
			return(found && (!enclosed || isalnum((unsigned char)*q)));
		}
	}

	return(found && !enclosed);
}

/*
 * Convert a string to a long that may not be <0.
 * If the string is invalid, or is less than 0, return -1.
 */
int
mandoc_strntoi(const char *p, size_t sz, int base)
{
	char		 buf[32];
	char		*ep;
	long		 v;

	if (sz > 31)
		return(-1);

	memcpy(buf, p, sz);
	buf[(int)sz] = '\0';

	errno = 0;
	v = strtol(buf, &ep, base);

	if (buf[0] == '\0' || *ep != '\0')
		return(-1);

	if (v > INT_MAX)
		v = INT_MAX;
	if (v < INT_MIN)
		v = INT_MIN;

	return((int)v);
}
