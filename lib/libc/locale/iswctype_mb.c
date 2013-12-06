/* $NetBSD: iswctype_mb.c,v 1.13 2013/05/17 12:55:57 joerg Exp $ */

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
__RCSID("$NetBSD: iswctype_mb.c,v 1.13 2013/05/17 12:55:57 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "setlocale_local.h"

#include "runetype_local.h"
#include "_wctype_local.h"
#include "_wctrans_local.h"

#define _RUNE_LOCALE(loc) ((_RuneLocale const *) \
    (loc)->part_impl[(size_t)LC_CTYPE])

#define _ISWCTYPE_FUNC(name, index)			\
int							\
isw##name##_l(wint_t wc, locale_t loc)			\
{							\
	_RuneLocale const *rl;				\
	_WCTypeEntry const *te;				\
							\
	rl = _RUNE_LOCALE(loc);				\
	te = &rl->rl_wctype[index];			\
	return _iswctype_priv(rl, wc, te);		\
}							\
int							\
isw##name(wint_t wc)					\
{							\
	return isw##name##_l(wc, _current_locale());	\
}

_ISWCTYPE_FUNC(alnum,  _WCTYPE_INDEX_ALNUM)
_ISWCTYPE_FUNC(alpha,  _WCTYPE_INDEX_ALPHA)
_ISWCTYPE_FUNC(blank,  _WCTYPE_INDEX_BLANK)
_ISWCTYPE_FUNC(cntrl,  _WCTYPE_INDEX_CNTRL)
_ISWCTYPE_FUNC(digit,  _WCTYPE_INDEX_DIGIT)
_ISWCTYPE_FUNC(graph,  _WCTYPE_INDEX_GRAPH)
_ISWCTYPE_FUNC(lower,  _WCTYPE_INDEX_LOWER)
_ISWCTYPE_FUNC(print,  _WCTYPE_INDEX_PRINT)
_ISWCTYPE_FUNC(punct,  _WCTYPE_INDEX_PUNCT)
_ISWCTYPE_FUNC(space,  _WCTYPE_INDEX_SPACE)
_ISWCTYPE_FUNC(upper,  _WCTYPE_INDEX_UPPER)
_ISWCTYPE_FUNC(xdigit, _WCTYPE_INDEX_XDIGIT)

#define _TOWCTRANS_FUNC(name, index)			\
wint_t							\
tow##name##_l(wint_t wc, locale_t loc)			\
{							\
	_RuneLocale const *rl;				\
	_WCTransEntry const *te;			\
							\
	rl = _RUNE_LOCALE(loc);				\
	te = &rl->rl_wctrans[index];			\
	return _towctrans_priv(wc, te);			\
}							\
wint_t							\
tow##name(wint_t wc)					\
{							\
	return tow##name##_l(wc, _current_locale());	\
}
_TOWCTRANS_FUNC(upper, _WCTRANS_INDEX_UPPER)
_TOWCTRANS_FUNC(lower, _WCTRANS_INDEX_LOWER)

wctype_t
wctype_l(const char *charclass, locale_t loc)
{
	_RuneLocale const *rl;
	size_t i;

	rl = _RUNE_LOCALE(loc);
	for (i = 0; i < _WCTYPE_NINDEXES; ++i) {
		if (!strcmp(rl->rl_wctype[i].te_name, charclass))
			return (wctype_t)__UNCONST(&rl->rl_wctype[i]);
	}
	return (wctype_t)NULL;
}

wctype_t
wctype(const char *charclass)
{
	return wctype_l(charclass, _current_locale());
}

wctrans_t
wctrans_l(const char *charmap, locale_t loc)
{
	_RuneLocale const *rl;
	size_t i;

	rl = _RUNE_LOCALE(loc);
	for (i = 0; i < _WCTRANS_NINDEXES; ++i) {
		_DIAGASSERT(rl->rl_wctrans[i].te_name != NULL);
		if (!strcmp(rl->rl_wctrans[i].te_name, charmap))
			return (wctrans_t)__UNCONST(&rl->rl_wctype[i]);
	}
	return (wctrans_t)NULL;
}

wctrans_t
wctrans(const char *charmap)
{
	return wctrans_l(charmap, _current_locale());
}

int
iswctype_l(wint_t wc, wctype_t charclass, locale_t loc)
{
	_RuneLocale const *rl;
	_WCTypeEntry const *te;

	if (charclass == NULL) {
		errno = EINVAL;
		return 0;
	}

	rl = _RUNE_LOCALE(loc);
	te = (_WCTypeEntry const *)(void *)charclass;
	return _iswctype_priv(rl, wc, te);
}

int
iswctype(wint_t wc, wctype_t charclass)
{
	return iswctype_l(wc, charclass, _current_locale());
}

wint_t
towctrans(wint_t wc, wctrans_t charmap)
{
	_WCTransEntry const *te;

	if (charmap == NULL) {
		errno = EINVAL;
		return wc;
	}
	te = (_WCTransEntry const *)(void *)charmap;
	return _towctrans_priv(wc, te);
}

/* ARGSUSED */
wint_t
towctrans_l(wint_t wc, wctrans_t charmap, locale_t loc)
{
	return towctrans(wc, charmap);
}

__weak_alias(wcwidth,_wcwidth)
__weak_alias(wcwidth_l,_wcwidth_l)

int
wcwidth_l(wchar_t wc, locale_t loc)
{
	_RuneLocale const *rl;
	_RuneType x;

	if (wc == L'\0')
		return 0;

	rl = _RUNE_LOCALE(loc);
	x = _runetype_priv(rl, wc);
	if (x & _RUNETYPE_R)
		return ((unsigned)x & _RUNETYPE_SWM) >> _RUNETYPE_SWS;
	return -1;
}

int
wcwidth(wchar_t wc)
{
	return wcwidth_l(wc, _current_locale());
}

int
wcswidth_l(const wchar_t * __restrict ws, size_t wn, locale_t loc)
{
	_RuneLocale const *rl;
	_RuneType x;
	int width;

	_DIAGASSERT(ws != NULL);

	rl = _RUNE_LOCALE(loc);
	width = 0;
	while (wn > 0 && *ws != L'\0') {
		x = _runetype_priv(rl, *ws);
		if ((x & _RUNETYPE_R) == 0)
			return -1;
		width += ((unsigned)x & _RUNETYPE_SWM) >> _RUNETYPE_SWS;
		++ws, --wn;
	}
	return width;
}

int
wcswidth(const wchar_t * __restrict ws, size_t wn)
{
	return wcswidth_l(ws, wn, _current_locale());
}
