/*	$NetBSD: strxfrm.c,v 1.14 2013/05/17 12:55:57 joerg Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
#if 0
static char sccsid[] = "@(#)strxfrm.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: strxfrm.c,v 1.14 2013/05/17 12:55:57 joerg Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <locale.h>
#include <string.h>
#include "setlocale_local.h"

__weak_alias(strxfrm_l, _strxfrm_l)

/*
 * Transform src, storing the result in dst, such that
 * strcmp() on transformed strings returns what strcoll()
 * on the original untransformed strings would return.
 */
size_t
strxfrm_l(char *dst, const char *src, size_t n, locale_t loc)
{
	size_t srclen, copysize;

	_DIAGASSERT(src != NULL);

	/* XXX: LC_COLLATE should be implemented. */
	/* LINTED */(void)loc;

	/*
	 * Since locales are unimplemented, this is just a copy.
	 */
	srclen = strlen(src);
	if (n != 0) {
		_DIAGASSERT(dst != NULL);
		copysize = srclen < n ? srclen : n - 1;
		(void)memcpy(dst, src, copysize);
		dst[copysize] = 0;
	}
	return (srclen);
}

size_t
strxfrm(char *dst, const char *src, size_t n)
{
	return strxfrm_l(dst, src, n, _current_locale());
}
