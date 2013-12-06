/*	$NetBSD: citrus_ctype_fallback.c,v 1.3 2013/05/28 16:57:56 joerg Exp $	*/

/*-
 * Copyright (c)2003 Citrus Project,
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
__RCSID("$NetBSD: citrus_ctype_fallback.c,v 1.3 2013/05/28 16:57:56 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "citrus_module.h"
#include "citrus_ctype.h"
#include "citrus_ctype_fallback.h"

/*
 * for ABI version >= 0x00000002
 */ 

int
_citrus_ctype_btowc_fallback(_citrus_ctype_rec_t * __restrict cc,
			     int c, wint_t * __restrict wcresult)
{
	char mb;
	/*
	 * what we need is _PRIVSIZE
	 * and we know that it's smaller than sizeof(mbstate_t).
	 */
	char pspriv[sizeof(mbstate_t)];
	wchar_t wc;
	size_t nr;
	int err;

	_DIAGASSERT(cc != NULL && cc->cc_closure != NULL);

	if (c == EOF) {
		*wcresult = WEOF;
		return 0;
	}

	memset(&pspriv, 0, sizeof(pspriv));
	mb = (char)(unsigned)c;
	err = _citrus_ctype_mbrtowc(cc, &wc, &mb, 1, (void *)&pspriv, &nr);
	if (!err && (nr == 0 || nr == 1))
		*wcresult = wc;
	else
		*wcresult = WEOF;

	return 0;
}

int
_citrus_ctype_wctob_fallback(_citrus_ctype_rec_t * __restrict cc,
			     wint_t wc, int * __restrict cresult)
{
	/*
	 * what we need is _PRIVSIZE
	 * and we know that it's smaller than sizeof(mbstate_t).
	 */
	char pspriv[sizeof(mbstate_t)];
	char buf[MB_LEN_MAX];
	size_t nr;
	int err;

	_DIAGASSERT(cc != NULL && cc->cc_closure != NULL);

	if (wc == WEOF) {
		*cresult = EOF;
		return 0;
	}
	memset(&pspriv, 0, sizeof(pspriv));
	err = _citrus_ctype_wcrtomb(cc, buf, (wchar_t)wc, (void *)&pspriv, &nr);
	if (!err && nr == 1)
		*cresult = buf[0];
	else
		*cresult = EOF;

	return 0;
}

/*
 * for ABI version >= 0x00000003
 */ 

int
_citrus_ctype_mbsnrtowcs_fallback(_citrus_ctype_rec_t * __restrict cc,
    wchar_t * __restrict pwcs, const char ** __restrict s, size_t in,
    size_t n, void * __restrict psenc, size_t * __restrict nresult)
{
	int err;
	size_t cnt, siz;
	const char *s0, *se;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(psenc != NULL);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(*s != NULL);

	/* if pwcs is NULL, ignore n */
	if (pwcs == NULL)
		n = 1; /* arbitrary >0 value */

	err = 0;
	cnt = 0;
	se = *s + in;
	s0 = *s; /* to keep *s unchanged for now, use copy instead. */
	while (s0 < se && n > 0) {
		err = _citrus_ctype_mbrtowc(cc, pwcs, s0, (size_t)(se - s0),
		    psenc, &siz);
		if (err) {
			cnt = (size_t)-1;
			goto bye;
		}
		if (siz == (size_t)-2) {
			s0 = se;
			goto bye;
		}
		switch (siz) {
		case 0:
			if (pwcs) {
				size_t dum;
				_citrus_ctype_mbrtowc(cc, NULL, NULL, 0, psenc,
				    &dum);
			}
			s0 = 0;
			goto bye;
		default:
			if (pwcs) {
				pwcs++;
				n--;
			}
			s0 += siz;
			cnt++;
			break;
		}
	}
bye:
	if (pwcs)
		*s = s0;

	*nresult = cnt;

	return err;
}

int
_citrus_ctype_wcsnrtombs_fallback(_citrus_ctype_rec_t * __restrict cc,
    char * __restrict s, const wchar_t ** __restrict pwcs, size_t in,
    size_t n, void * __restrict psenc, size_t * __restrict nresult)
{
	size_t cnt = 0;
	int err;
	char buf[MB_LEN_MAX];
	size_t siz;
	const wchar_t* pwcs0;
	mbstate_t state;

	pwcs0 = *pwcs;

	if (!s)
		n = 1;

	while (in > 0 && n > 0) {
		memcpy(&state, psenc, sizeof(state));
		err = _citrus_ctype_wcrtomb(cc, buf, *pwcs0, psenc, &siz);
		if (siz == (size_t)-1) {
			*nresult = siz;
			return (err);
		}

		if (s) {
			if (n < siz) {
				memcpy(psenc, &state, sizeof(state));
				break;
			}
			memcpy(s, buf, siz);
			s += siz;
			n -= siz;
		}
		cnt += siz;
		if (!*pwcs0) {
			if (s) {
				memset(psenc, 0, sizeof(state));
			}
			pwcs0 = 0;
			cnt--; /* don't include terminating null */
			break;
		}
		pwcs0++;
		--in;
	}
	if (s)
		*pwcs = pwcs0;

	*nresult = cnt;
	return (0);
}
