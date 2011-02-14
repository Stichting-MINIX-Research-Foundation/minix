/*	$NetBSD: citrus_utf8.c,v 1.17 2008/06/14 16:01:08 tnozaki Exp $	*/

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
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: citrus_utf8.c,v 1.17 2008/06/14 16:01:08 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <wchar.h>
#include <sys/types.h>
#include <limits.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_module.h"
#include "citrus_ctype.h"
#include "citrus_stdenc.h"
#include "citrus_utf8.h"


/* ----------------------------------------------------------------------
 * private stuffs used by templates
 */

static int _UTF8_count_array[256];
static int const *_UTF8_count = NULL;

static const u_int32_t _UTF8_range[] = {
	0,	/*dummy*/
	0x00000000, 0x00000080, 0x00000800, 0x00010000, 
	0x00200000, 0x04000000, 0x80000000,
};

typedef struct {
	char ch[6];
	int chlen;
} _UTF8State;

typedef struct {
} _UTF8EncodingInfo;

typedef struct {
	_UTF8EncodingInfo	ei;
	struct {
		/* for future multi-locale facility */
		_UTF8State	s_mblen;
		_UTF8State	s_mbrlen;
		_UTF8State	s_mbrtowc;
		_UTF8State	s_mbtowc;
		_UTF8State	s_mbsrtowcs;
		_UTF8State	s_wcrtomb;
		_UTF8State	s_wcsrtombs;
		_UTF8State	s_wctomb;
	} states;
} _UTF8CTypeInfo;

#define _CEI_TO_EI(_cei_)		(&(_cei_)->ei)
#define _CEI_TO_STATE(_ei_, _func_)	(_ei_)->states.s_##_func_

#define _FUNCNAME(m)			_citrus_UTF8_##m
#define _ENCODING_INFO			_UTF8EncodingInfo
#define _CTYPE_INFO			_UTF8CTypeInfo
#define _ENCODING_STATE			_UTF8State
#define _ENCODING_MB_CUR_MAX(_ei_)	6
#define _ENCODING_IS_STATE_DEPENDENT	0
#define _STATE_NEEDS_EXPLICIT_INIT(_ps_)	0


static __inline void
_UTF8_init_count(void)
{
	int i;
	if (!_UTF8_count) {
		memset(_UTF8_count_array, 0, sizeof(_UTF8_count_array));
		for (i = 0; i <= 0x7f; i++)
			_UTF8_count_array[i] = 1;
		for (i = 0xc0; i <= 0xdf; i++)
			_UTF8_count_array[i] = 2;
		for (i = 0xe0; i <= 0xef; i++)
			_UTF8_count_array[i] = 3;
		for (i = 0xf0; i <= 0xf7; i++)
			_UTF8_count_array[i] = 4;
		for (i = 0xf8; i <= 0xfb; i++)
			_UTF8_count_array[i] = 5;
		for (i = 0xfc; i <= 0xfd; i++)
			_UTF8_count_array[i] = 6;
		_UTF8_count = _UTF8_count_array;
	}
}

static int
_UTF8_findlen(wchar_t v)
{
	int i;
	u_int32_t c;

	c = (u_int32_t)v;	/*XXX*/
	for (i = 1; i < sizeof(_UTF8_range) / sizeof(_UTF8_range[0]) - 1; i++)
		if (c >= _UTF8_range[i] && c < _UTF8_range[i + 1])
			return i;

	return -1;	/*out of range*/
}

static __inline int
_UTF8_surrogate(wchar_t wc)
{
	return wc >= 0xd800 && wc <= 0xdfff;
}

static __inline void
/*ARGSUSED*/
_citrus_UTF8_init_state(_UTF8EncodingInfo *ei, _UTF8State *s)
{
	s->chlen = 0;
}

static __inline void
/*ARGSUSED*/
_citrus_UTF8_pack_state(_UTF8EncodingInfo *ei, void *pspriv,
			const _UTF8State *s)
{
	memcpy(pspriv, (const void *)s, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_UTF8_unpack_state(_UTF8EncodingInfo *ei, _UTF8State *s,
			  const void *pspriv)
{
	memcpy((void *)s, pspriv, sizeof(*s));
}

static int
_citrus_UTF8_mbrtowc_priv(_UTF8EncodingInfo *ei, wchar_t *pwc, const char **s,
			  size_t n, _UTF8State *psenc, size_t *nresult)
{
	wchar_t wchar;
	const char *s0;
	int c;
	int i;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(psenc != NULL);

	s0 = *s;

	if (s0 == NULL) {
		_citrus_UTF8_init_state(ei, psenc);
		*nresult = 0; /* state independent */
		return 0;
	}

	/* make sure we have the first byte in the buffer */
	if (psenc->chlen == 0) {
		if (n-- < 1)
			goto restart;
		psenc->ch[psenc->chlen++] = *s0++;
	}

	c = _UTF8_count[psenc->ch[0] & 0xff];
	if (c < 1 || c < psenc->chlen)
		goto ilseq;

	if (c == 1)
		wchar = psenc->ch[0] & 0xff;
	else {
		while (psenc->chlen < c) {
			if (n-- < 1)
				goto restart;
			psenc->ch[psenc->chlen++] = *s0++;
		}
		wchar = psenc->ch[0] & (0x7f >> c);
		for (i = 1; i < c; i++) {
			if ((psenc->ch[i] & 0xc0) != 0x80)
				goto ilseq;
			wchar <<= 6;
			wchar |= (psenc->ch[i] & 0x3f);
		}
		if (_UTF8_surrogate(wchar) || _UTF8_findlen(wchar) != c)
			goto ilseq;
	}
	if (pwc != NULL)
		*pwc = wchar;
	*nresult = (wchar == 0) ? 0 : s0 - *s;
	*s = s0;
	psenc->chlen = 0;

	return 0;

ilseq:
	*nresult = (size_t)-1;
	return EILSEQ;

restart:
	*s = s0;
	*nresult = (size_t)-2;
	return 0;
}

static int
_citrus_UTF8_wcrtomb_priv(_UTF8EncodingInfo *ei, char *s, size_t n, wchar_t wc,
			  _UTF8State *psenc, size_t *nresult)
{
	int cnt, i, ret;
	wchar_t c;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(s != NULL);

	if (_UTF8_surrogate(wc)) {
		ret = EILSEQ;
		goto err;
	}
	cnt = _UTF8_findlen(wc);
	if (cnt <= 0 || cnt > 6) {
		/* invalid UCS4 value */
		ret = EILSEQ;
		goto err;
	}
	if (n < cnt) {
		/* bound check failure */
		ret = E2BIG;
		goto err;
	}

	c = wc;
	if (s) {
		for (i = cnt - 1; i > 0; i--) {
			s[i] = 0x80 | (c & 0x3f);
			c >>= 6;
		}
		s[0] = c;
		if (cnt == 1)
			s[0] &= 0x7f;
		else {
			s[0] &= (0x7f >> cnt);
			s[0] |= ((0xff00 >> cnt) & 0xff);
		}
	}

	*nresult = (size_t)cnt;
	return 0;

err:
	*nresult = (size_t)-1;
	return ret;
}

static __inline int
/*ARGSUSED*/
_citrus_UTF8_stdenc_wctocs(_UTF8EncodingInfo * __restrict ei,
			   _csid_t * __restrict csid,
			   _index_t * __restrict idx,
			   wchar_t wc)
{

	_DIAGASSERT(csid != NULL && idx != NULL);

	*csid = 0;
	*idx = (_citrus_index_t)wc;

	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_UTF8_stdenc_cstowc(_UTF8EncodingInfo * __restrict ei,
			   wchar_t * __restrict wc,
			   _csid_t csid, _index_t idx)
{

	_DIAGASSERT(wc != NULL);

	if (csid != 0)
		return (EILSEQ);

	*wc = (wchar_t)idx;

	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_UTF8_stdenc_get_state_desc_generic(_UTF8EncodingInfo * __restrict ei,
					   _UTF8State * __restrict psenc,
					   int * __restrict rstate)
{

	if (psenc->chlen == 0)
		*rstate = _STDENC_SDGEN_INITIAL;
	else
		*rstate = _STDENC_SDGEN_INCOMPLETE_CHAR;

	return 0;
}

static int
/*ARGSUSED*/
_citrus_UTF8_encoding_module_init(_UTF8EncodingInfo * __restrict ei,
				  const void * __restrict var, size_t lenvar)
{
	_UTF8_init_count();

	return 0;
}

static void
/*ARGSUSED*/
_citrus_UTF8_encoding_module_uninit(_UTF8EncodingInfo *ei)
{
}


/* ----------------------------------------------------------------------
 * public interface for ctype
 */

_CITRUS_CTYPE_DECLS(UTF8);
_CITRUS_CTYPE_DEF_OPS(UTF8);

#include "citrus_ctype_template.h"

/* ----------------------------------------------------------------------
 * public interface for stdenc
 */

_CITRUS_STDENC_DECLS(UTF8);
_CITRUS_STDENC_DEF_OPS(UTF8);

#include "citrus_stdenc_template.h"
