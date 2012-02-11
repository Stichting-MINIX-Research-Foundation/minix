/*	$NetBSD: citrus_ctype.h,v 1.2 2003/03/05 20:18:15 tshiozak Exp $	*/

/*-
 * Copyright (c)2002 Citrus Project,
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
 *
 */

#ifndef _CITRUS_CTYPE_H_
#define _CITRUS_CTYPE_H_

#include "citrus_ctype_local.h"

typedef struct _citrus_ctype_rec *_citrus_ctype_t;

__BEGIN_DECLS
int _citrus_ctype_open(_citrus_ctype_t * __restrict,
		       char const * __restrict, void * __restrict,
		       size_t, size_t);
void _citrus_ctype_close(_citrus_ctype_t);
__END_DECLS

static __inline unsigned
_citrus_ctype_get_mb_cur_max(_citrus_ctype_t cc)
{

	_DIAGASSERT(cc && cc->cc_ops);
	return (*cc->cc_ops->co_get_mb_cur_max)(cc->cc_closure);
}

static __inline int
_citrus_ctype_mblen(_citrus_ctype_t cc, const char *s, size_t n, int *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mblen && nresult);
	return (*cc->cc_ops->co_mblen)(cc->cc_closure, s, n, nresult);
}

static __inline int
_citrus_ctype_mbrlen(_citrus_ctype_t cc, const char *s, size_t n,
		     void *pspriv, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mbrlen && nresult);
	return (*cc->cc_ops->co_mbrlen)(cc->cc_closure, s, n, pspriv, nresult);
}

static __inline int
_citrus_ctype_mbrtowc(_citrus_ctype_t cc, wchar_t *pwc, const char *s,
		      size_t n, void *pspriv, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mbrtowc);
	return (*cc->cc_ops->co_mbrtowc)(cc->cc_closure, pwc, s, n, pspriv,
					 nresult);
}

static __inline int
_citrus_ctype_mbsinit(_citrus_ctype_t cc, void const *pspriv, int *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mbsinit && nresult);
	return (*cc->cc_ops->co_mbsinit)(cc->cc_closure, pspriv, nresult);
}

static __inline int
_citrus_ctype_mbsrtowcs(_citrus_ctype_t cc, wchar_t *pwcs, const char **s,
			size_t n, void *pspriv, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mbsrtowcs && nresult);
	return (*cc->cc_ops->co_mbsrtowcs)(cc->cc_closure, pwcs, s, n, pspriv,
					   nresult);
}

static __inline int
_citrus_ctype_mbstowcs(_citrus_ctype_t cc, wchar_t *pwcs, const char *s,
		       size_t n, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mbstowcs && nresult);
	return (*cc->cc_ops->co_mbstowcs)(cc->cc_closure, pwcs, s, n, nresult);
}

static __inline int
_citrus_ctype_mbtowc(_citrus_ctype_t cc, wchar_t *pw, const char *s, size_t n,
		     int *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_mbtowc && nresult);
	return (*cc->cc_ops->co_mbtowc)(cc->cc_closure, pw, s, n, nresult);
}

static __inline int
_citrus_ctype_wcrtomb(_citrus_ctype_t cc, char *s, wchar_t wc,
		      void *pspriv, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_wcrtomb && nresult);
	return (*cc->cc_ops->co_wcrtomb)(cc->cc_closure, s, wc, pspriv,
					 nresult);
}

static __inline int
_citrus_ctype_wcsrtombs(_citrus_ctype_t cc, char *s, const wchar_t **ppwcs,
			size_t n, void *pspriv, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_wcsrtombs && nresult);
	return (*cc->cc_ops->co_wcsrtombs)(cc->cc_closure, s, ppwcs, n,
					   pspriv, nresult);
}

static __inline int
_citrus_ctype_wcstombs(_citrus_ctype_t cc, char *s, const wchar_t *wcs,
		       size_t n, size_t *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_wcstombs && nresult);
	return (*cc->cc_ops->co_wcstombs)(cc->cc_closure, s, wcs, n, nresult);
}

static __inline int
_citrus_ctype_wctomb(_citrus_ctype_t cc, char *s, wchar_t wc, int *nresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_wctomb && nresult);
	return (*cc->cc_ops->co_wctomb)(cc->cc_closure, s, wc, nresult);
}

static __inline int
_citrus_ctype_btowc(_citrus_ctype_t cc, int c, wint_t *wcresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_btowc && wcresult);
	return (*cc->cc_ops->co_btowc)(cc, c, wcresult);
}

static __inline int
_citrus_ctype_wctob(_citrus_ctype_t cc, wint_t c, int *cresult)
{

	_DIAGASSERT(cc && cc->cc_ops && cc->cc_ops->co_wctob && cresult);
	return (*cc->cc_ops->co_wctob)(cc, c, cresult);
}

extern _citrus_ctype_rec_t _citrus_ctype_default;

#endif
