/*	$NetBSD: citrus_ctype_fallback.c,v 1.2 2003/06/27 14:52:25 yamt Exp $	*/

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
__RCSID("$NetBSD: citrus_ctype_fallback.c,v 1.2 2003/06/27 14:52:25 yamt Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <assert.h>
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
