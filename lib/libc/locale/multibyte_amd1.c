/*	$NetBSD: multibyte_amd1.c,v 1.14 2013/08/18 20:03:48 joerg Exp $	*/

/*-
 * Copyright (c)2002, 2008 Citrus Project,
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
__RCSID("$NetBSD: multibyte_amd1.c,v 1.14 2013/08/18 20:03:48 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <langinfo.h>
#include <stdlib.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <wchar.h>

#include "setlocale_local.h"

#include "citrus_module.h"
#include "citrus_ctype.h"
#include "runetype_local.h"
#include "multibyte.h"

size_t
mbrlen_l(const char *s, size_t n, mbstate_t *ps, locale_t loc)
{
	size_t ret;
	int err0;

	_fixup_ps(_RUNE_LOCALE(loc), ps, s == NULL);

	err0 = _citrus_ctype_mbrlen(_ps_to_ctype(ps, loc), s, n,
				     _ps_to_private(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

size_t
mbrlen(const char *s, size_t n, mbstate_t *ps)
{
	return mbrlen_l(s, n, ps, _current_locale());
}

int
mbsinit_l(const mbstate_t *ps, locale_t loc)
{
	int ret;
	int err0;
	_RuneLocale *rl;

	if (ps == NULL)
		return 1;

	if (_ps_to_runelocale(ps) == NULL)
		rl = _RUNE_LOCALE(loc);
	else
		rl = _ps_to_runelocale(ps);

	/* mbsinit should cause no error... */
	err0 = _citrus_ctype_mbsinit(rl->rl_citrus_ctype,
				      _ps_to_private_const(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

int
mbsinit(const mbstate_t *ps)
{
	return mbsinit_l(ps, _current_locale());
}

size_t
mbrtowc_l(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps, locale_t loc)
{
	size_t ret;
	int err0;

	_fixup_ps(_RUNE_LOCALE(loc), ps, s == NULL);

	err0 = _citrus_ctype_mbrtowc(_ps_to_ctype(ps, loc), pwc, s, n,
				      _ps_to_private(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

size_t
mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
	return mbrtowc_l(pwc, s, n, ps, _current_locale());
}

size_t
mbsrtowcs_l(wchar_t *pwcs, const char **s, size_t n, mbstate_t *ps,
    locale_t loc)
{
	size_t ret;
	int err0;

	_fixup_ps(_RUNE_LOCALE(loc), ps, s == NULL);

	err0 = _citrus_ctype_mbsrtowcs(_ps_to_ctype(ps, loc), pwcs, s, n,
					_ps_to_private(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

size_t
mbsrtowcs(wchar_t *pwcs, const char **s, size_t n, mbstate_t *ps)
{
	return mbsrtowcs_l(pwcs, s, n, ps, _current_locale());
}

size_t
mbsnrtowcs_l(wchar_t *pwcs, const char **s, size_t in, size_t n, mbstate_t *ps,
    locale_t loc)
{
	size_t ret;
	int err0;

	_fixup_ps(_RUNE_LOCALE(loc), ps, s == NULL);

	err0 = _citrus_ctype_mbsnrtowcs(_ps_to_ctype(ps, loc), pwcs, s, in, n,
					_ps_to_private(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

size_t
mbsnrtowcs(wchar_t *pwcs, const char **s, size_t in, size_t n, mbstate_t *ps)
{
	return mbsnrtowcs_l(pwcs, s, in, n, ps, _current_locale());
}

size_t
wcrtomb_l(char *s, wchar_t wc, mbstate_t *ps, locale_t loc)
{
	size_t ret;
	int err0;

	_fixup_ps(_RUNE_LOCALE(loc), ps, s == NULL);

	err0 = _citrus_ctype_wcrtomb(_ps_to_ctype(ps, loc), s, wc,
				       _ps_to_private(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

size_t
wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	return wcrtomb_l(s, wc, ps, _current_locale());
}

size_t
wcsrtombs_l(char *s, const wchar_t **ppwcs, size_t n, mbstate_t *ps,
    locale_t loc)
{
	size_t ret;
	int err0;

	_fixup_ps(_RUNE_LOCALE(loc), ps, s == NULL);

	err0 = _citrus_ctype_wcsrtombs(_ps_to_ctype(ps, loc), s, ppwcs, n,
					_ps_to_private(ps), &ret);
	if (err0)
		errno = err0;

	return ret;
}

size_t
wcsrtombs(char *s, const wchar_t **ppwcs, size_t n, mbstate_t *ps)
{
	return wcsrtombs_l(s, ppwcs, n, ps, _current_locale());
}

wint_t
btowc_l(int c, locale_t loc)
{
	wint_t ret;
	int err0;

	err0 = _citrus_ctype_btowc(_CITRUS_CTYPE(loc), c, &ret);
	if (err0)
		errno = err0;

	return ret;
}

wint_t
btowc(int c)
{
	return btowc_l(c, _current_locale());
}

int
wctob_l(wint_t wc, locale_t loc)
{
	int ret;
	int err0;

	err0 = _citrus_ctype_wctob(_CITRUS_CTYPE(loc), wc, &ret);
	if (err0)
		errno = err0;

	return ret;
}

int
wctob(wint_t wc)
{
	return wctob_l(wc, _current_locale());
}

size_t
_mb_cur_max_l(locale_t loc)
{

	return _citrus_ctype_get_mb_cur_max(_CITRUS_CTYPE(loc));
}
