/*	$NetBSD: citrus_euctw.c,v 1.12 2013/05/28 16:57:56 joerg Exp $	*/

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
 * Copyright (c)1999 Citrus Project,
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
 *	$Citrus: xpg4dl/FreeBSD/lib/libc/locale/euctw.c,v 1.13 2001/06/21 01:51:44 yamt Exp $
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: citrus_euctw.c,v 1.12 2013/05/28 16:57:56 joerg Exp $");
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
#include "citrus_euctw.h"


/* ----------------------------------------------------------------------
 * private stuffs used by templates
 */

typedef struct {
	char ch[4];
	int chlen;
} _EUCTWState;

typedef struct {
	int dummy;
} _EUCTWEncodingInfo;
typedef struct {
	_EUCTWEncodingInfo	ei;
	struct {
		/* for future multi-locale facility */
		_EUCTWState	s_mblen;
		_EUCTWState	s_mbrlen;
		_EUCTWState	s_mbrtowc;
		_EUCTWState	s_mbtowc;
		_EUCTWState	s_mbsrtowcs;
		_EUCTWState	s_mbsnrtowcs;
		_EUCTWState	s_wcrtomb;
		_EUCTWState	s_wcsrtombs;
		_EUCTWState	s_wcsnrtombs;
		_EUCTWState	s_wctomb;
	} states;
} _EUCTWCTypeInfo;

#define	_SS2	0x008e
#define	_SS3	0x008f

#define _CEI_TO_EI(_cei_)		(&(_cei_)->ei)
#define _CEI_TO_STATE(_cei_, _func_)	(_cei_)->states.s_##_func_

#define _FUNCNAME(m)			_citrus_EUCTW_##m
#define _ENCODING_INFO			_EUCTWEncodingInfo
#define _CTYPE_INFO			_EUCTWCTypeInfo
#define _ENCODING_STATE			_EUCTWState
#define _ENCODING_MB_CUR_MAX(_ei_)	4
#define _ENCODING_IS_STATE_DEPENDENT	0
#define _STATE_NEEDS_EXPLICIT_INIT(_ps_)	0

static __inline int
_citrus_EUCTW_cs(u_int c)
{
	c &= 0xff;

	return ((c & 0x80) ? (c == _SS2 ? 2 : 1) : 0);
}

static __inline int
_citrus_EUCTW_count(int cs)
{
	switch (cs) {
	case 0:
		return 1;
	case 1:
		return 2;
	case 2:
		return 4;
	case 3:
		abort();
		/*NOTREACHED*/
	}
	return 0;
}

static __inline void
/*ARGSUSED*/
_citrus_EUCTW_init_state(_EUCTWEncodingInfo * __restrict ei,
			 _EUCTWState * __restrict s)
{
	memset(s, 0, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_EUCTW_pack_state(_EUCTWEncodingInfo * __restrict ei,
			 void * __restrict pspriv,
			 const _EUCTWState * __restrict s)
{
	memcpy(pspriv, (const void *)s, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_EUCTW_unpack_state(_EUCTWEncodingInfo * __restrict ei,
			   _EUCTWState * __restrict s,
			   const void * __restrict pspriv)
{
	memcpy((void *)s, pspriv, sizeof(*s));
}

static int
/*ARGSUSED*/
_citrus_EUCTW_encoding_module_init(_EUCTWEncodingInfo * __restrict ei,
				   const void * __restrict var, size_t lenvar)
{

	_DIAGASSERT(ei != NULL);

	memset((void *)ei, 0, sizeof(*ei));

	return 0;
}

static void
/*ARGSUSED*/
_citrus_EUCTW_encoding_module_uninit(_EUCTWEncodingInfo *ei)
{
}

static int
_citrus_EUCTW_mbrtowc_priv(_EUCTWEncodingInfo * __restrict ei,
			   wchar_t * __restrict pwc,
			   const char ** __restrict s,
			   size_t n, _EUCTWState * __restrict psenc,
			   size_t * __restrict nresult)
{
	wchar_t wchar;
	int c, cs;
	int chlenbak;
	const char *s0;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(psenc != NULL);
	_DIAGASSERT(s != NULL);

	s0 = *s;

	if (s0 == NULL) {
		_citrus_EUCTW_init_state(ei, psenc);
		*nresult = 0; /* state independent */
		return (0);
	}

	chlenbak = psenc->chlen;

	/* make sure we have the first byte in the buffer */
	switch (psenc->chlen) {
	case 0:
		if (n < 1)
			goto restart;
		psenc->ch[0] = *s0++;
		psenc->chlen = 1;
		n--;
		break;
	case 1:
	case 2:
		break;
	default:
		/* illgeal state */
		goto ilseq;
	}

	c = _citrus_EUCTW_count(cs = _citrus_EUCTW_cs(psenc->ch[0] & 0xff));
	if (c == 0)
		goto ilseq;
	while (psenc->chlen < c) {
		if (n < 1)
			goto ilseq;
		psenc->ch[psenc->chlen] = *s0++;
		psenc->chlen++;
		n--;
	}

	wchar = 0;
	switch (cs) {
	case 0:
		if (psenc->ch[0] & 0x80)
			goto ilseq;
		wchar = psenc->ch[0] & 0xff;
		break;
	case 1:
		if (!(psenc->ch[0] & 0x80) || !(psenc->ch[1] & 0x80))
			goto ilseq;
		wchar = ((psenc->ch[0] & 0xff) << 8) | (psenc->ch[1] & 0xff);
		wchar |= 'G' << 24;
		break;
	case 2:
		if ((u_char)psenc->ch[1] < 0xa1 || 0xa7 < (u_char)psenc->ch[1])
			goto ilseq;
		if (!(psenc->ch[2] & 0x80) || !(psenc->ch[3] & 0x80))
			goto ilseq;
		wchar = ((psenc->ch[2] & 0xff) << 8) | (psenc->ch[3] & 0xff);
		wchar |= ('G' + psenc->ch[1] - 0xa1) << 24;
		break;
	default:
		goto ilseq;
	}

	*s = s0;
	psenc->chlen = 0;

	if (pwc)
		*pwc = wchar;

	if (!wchar)
		*nresult = 0;
	else
		*nresult = c - chlenbak;

	return (0);

ilseq:
	psenc->chlen = 0;
	*nresult = (size_t)-1;
	return (EILSEQ);

restart:
	*s = s0;
	*nresult = (size_t)-1;
	return (0);
}

static int
_citrus_EUCTW_wcrtomb_priv(_EUCTWEncodingInfo * __restrict ei,
			   char * __restrict s, size_t n, wchar_t wc,
			   _EUCTWState * __restrict psenc,
			   size_t * __restrict nresult)
{
	wchar_t cs = wc & 0x7f000080;
	wchar_t v;
	int i, len, clen, ret;

	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(s != NULL);

	clen = 1;
	if (wc & 0x00007f00)
		clen = 2;
	if ((wc & 0x007f0000) && !(wc & 0x00800000))
		clen = 3;

	if (clen == 1 && cs == 0x00000000) {
		/* ASCII */
		len = 1;
		if (n < len) {
			ret = E2BIG;
			goto err;
		}
		v = wc & 0x0000007f;
	} else if (clen == 2 && cs == ('G' << 24)) {
		/* CNS-11643-1 */
		len = 2;
		if (n < len) {
			ret = E2BIG;
			goto err;
		}
		v = wc & 0x00007f7f;
		v |= 0x00008080;
	} else if (clen == 2 && 'H' <= (cs >> 24) && (cs >> 24) <= 'M') {
		/* CNS-11643-[2-7] */
		len = 4;
		if (n < len) {
			ret = E2BIG;
			goto err;
		}
		*s++ = _SS2;
		*s++ = (cs >> 24) - 'H' + 0xa2;
		v = wc & 0x00007f7f;
		v |= 0x00008080;
	} else {
		ret = EILSEQ;
		goto err;
	}

	i = clen;
	while (i-- > 0)
		*s++ = (v >> (i << 3)) & 0xff;

	*nresult = len;
	return 0;

err:
	*nresult = (size_t)-1;
	return ret;
}

static __inline int
/*ARGSUSED*/
_citrus_EUCTW_stdenc_wctocs(_EUCTWEncodingInfo * __restrict ei,
			    _csid_t * __restrict csid,
			    _index_t * __restrict idx, wchar_t wc)
{

	_DIAGASSERT(ei != NULL && csid != NULL && idx != NULL);

	*csid = (_csid_t)(wc >> 24) & 0xFF;
	*idx  = (_index_t)(wc & 0x7F7F);

	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_EUCTW_stdenc_cstowc(_EUCTWEncodingInfo * __restrict ei,
			    wchar_t * __restrict wc,
			    _csid_t csid, _index_t idx)
{

	_DIAGASSERT(ei != NULL && wc != NULL);

	if (csid==0) {
		if ((idx & ~0x7F) != 0)
			return (EINVAL);
		*wc = (wchar_t)idx;
	} else {
		if (csid < 'G' || csid > 'M' || (idx & ~0x7F7F) != 0)
			return (EINVAL);
		*wc = (wchar_t)idx | ((wchar_t)csid<<24);
	}

	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_EUCTW_stdenc_get_state_desc_generic(_EUCTWEncodingInfo * __restrict ei,
					    _EUCTWState * __restrict psenc,
					    int * __restrict rstate)
{

	if (psenc->chlen == 0)
		*rstate = _STDENC_SDGEN_INITIAL;
	else
		*rstate = _STDENC_SDGEN_INCOMPLETE_CHAR;

	return 0;
}

/* ----------------------------------------------------------------------
 * public interface for ctype
 */

_CITRUS_CTYPE_DECLS(EUCTW);
_CITRUS_CTYPE_DEF_OPS(EUCTW);

#include "citrus_ctype_template.h"

/* ----------------------------------------------------------------------
 * public interface for stdenc
 */

_CITRUS_STDENC_DECLS(EUCTW);
_CITRUS_STDENC_DEF_OPS(EUCTW);

#include "citrus_stdenc_template.h"
