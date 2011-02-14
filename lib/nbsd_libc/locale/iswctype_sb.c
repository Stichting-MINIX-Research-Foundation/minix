/* $NetBSD: iswctype_sb.c,v 1.11 2010/06/01 18:00:28 tnozaki Exp $ */

/*-
 * Copyright (c)2008 Citrus Project,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: iswctype_sb.c,v 1.11 2010/06/01 18:00:28 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <assert.h>
#define _CTYPE_NOINLINE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define _ISWCTYPE_FUNC(name)				\
int							\
isw##name(wint_t wc)					\
{							\
	int c;						\
							\
	c = (wc == WEOF) ? EOF : (unsigned char)wc;	\
	return is##name(c);				\
}
_ISWCTYPE_FUNC(alnum)
_ISWCTYPE_FUNC(alpha)
_ISWCTYPE_FUNC(blank)
_ISWCTYPE_FUNC(cntrl)
_ISWCTYPE_FUNC(digit)
_ISWCTYPE_FUNC(graph)
_ISWCTYPE_FUNC(lower)
_ISWCTYPE_FUNC(print)
_ISWCTYPE_FUNC(punct)
_ISWCTYPE_FUNC(space)
_ISWCTYPE_FUNC(upper)
_ISWCTYPE_FUNC(xdigit)

#define _TOWCTRANS_FUNC(name)				\
wint_t							\
tow##name(wint_t wc)					\
{							\
	int c;						\
	c = (wc == WEOF) ? EOF : (unsigned char)wc;	\
	return to##name(c);				\
}
_TOWCTRANS_FUNC(upper)
_TOWCTRANS_FUNC(lower)

struct _wctype_priv_t {
	const char *name;
	int (*iswctype)(wint_t);
};

static const struct _wctype_priv_t _wctype_decl[] = {
    { "alnum",  &iswalnum  },
    { "alpha",  &iswalpha  },
    { "blank",  &iswblank  },
    { "cntrl",  &iswcntrl  },
    { "digit",  &iswdigit  },
    { "graph",  &iswgraph  },
    { "lower",  &iswlower  },
    { "print",  &iswprint  },
    { "punct",  &iswpunct  },
    { "space",  &iswspace  },
    { "upper",  &iswupper  },
    { "xdigit", &iswxdigit },
};
static const size_t _wctype_decl_size =
    sizeof(_wctype_decl) / sizeof(struct _wctype_priv_t);

wctype_t
wctype(const char *charclass)
{
	size_t i;

	for (i = 0; i < _wctype_decl_size; ++i) {
		 if (!strcmp(charclass, _wctype_decl[i].name))
			return (wctype_t)__UNCONST(&_wctype_decl[i]);
	}
	return (wctype_t)NULL;
}

struct _wctrans_priv_t {
	const char *name;
	wint_t (*towctrans)(wint_t);
};

static const struct _wctrans_priv_t _wctrans_decl[] = {
    { "upper", &towupper },
    { "lower", &towlower },
};
static const size_t _wctrans_decl_size =
    sizeof(_wctrans_decl) / sizeof(struct _wctrans_priv_t);

wctrans_t
/*ARGSUSED*/
wctrans(const char *charmap)
{
	size_t i;

	for (i = 0; i < _wctrans_decl_size; ++i) {
		 if (!strcmp(charmap, _wctrans_decl[i].name))
			return (wctrans_t)__UNCONST(&_wctrans_decl[i]);
	}
	return (wctrans_t)NULL;
}

int
/*ARGSUSED*/
iswctype(wint_t wc, wctype_t charclass)
{
	const struct _wctype_priv_t *p;

	p = (const struct _wctype_priv_t *)(void *)charclass;
	if (p < &_wctype_decl[0] || p > &_wctype_decl[_wctype_decl_size - 1]) {
		errno = EINVAL;
		return 0;
	}
	return (*p->iswctype)(wc);
}

wint_t
/*ARGSUSED*/
towctrans(wint_t wc, wctrans_t charmap)
{
	const struct _wctrans_priv_t *p;

	p = (const struct _wctrans_priv_t *)(void *)charmap;
	if (p < &_wctrans_decl[0] || p > &_wctrans_decl[_wctrans_decl_size - 1]) {
		errno = EINVAL;
		return wc;
	}
	return (*p->towctrans)(wc);
}

__weak_alias(wcwidth,_wcwidth)

int
wcwidth(wchar_t wc)
{ 
	int c;

	switch (wc) {
	case L'\0':
		return 0;
	case WEOF:
		c = EOF;
		break;
	default:
		c = (unsigned char)wc;
	}
	if (isprint(c))
		return 1;
	return -1;
}

int
wcswidth(const wchar_t * __restrict ws, size_t wn)
{
	const wchar_t *pws;
	int c;

	pws = ws;
	while (wn > 0 && *ws != L'\0') {
		c = (*ws == WEOF) ? EOF : (unsigned char)*ws;
		if (!isprint(c))
			return -1;
		++ws, --wn;
	}
	return (int)(ws - pws);
}
