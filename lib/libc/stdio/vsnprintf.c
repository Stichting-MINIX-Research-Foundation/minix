/*	$NetBSD: vsnprintf.c,v 1.27 2013/05/17 12:55:57 joerg Exp $	*/

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
static char sccsid[] = "@(#)vsnprintf.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: vsnprintf.c,v 1.27 2013/05/17 12:55:57 joerg Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include "reentrant.h"
#include "setlocale_local.h"
#include "local.h"

#if defined(_FORTIFY_SOURCE) && !defined(__lint__)
#undef vsnprintf
#define vsnprintf _vsnprintf
#undef snprintf
#define snprintf _snprintf
#endif

#ifdef __weak_alias
__weak_alias(vsnprintf,_vsnprintf)
__weak_alias(vsnprintf_l,_vsnprintf_l)
__weak_alias(snprintf,_snprintf)
__weak_alias(snprintf_l,_snprintf_l)
#endif

int
vsnprintf_l(char *str, size_t n, locale_t loc, const char *fmt, va_list ap)
{
	int ret;
	FILE f;
	struct __sfileext fext;
	unsigned char dummy[1];

	_DIAGASSERT(n == 0 || str != NULL);
	_DIAGASSERT(fmt != NULL);

	if ((int)n < 0) {
		errno = EINVAL;
		return -1;
	}

	_FILEEXT_SETUP(&f, &fext);
	f._file = -1;
	f._flags = __SWR | __SSTR;
	if (n == 0) {
		f._bf._base = f._p = dummy;
		f._bf._size = f._w = 0;
	} else {
		f._bf._base = f._p = (unsigned char *)str;
		_DIAGASSERT(__type_fit(int, n - 1));
		f._bf._size = f._w = (int)(n - 1);
	}
	ret = __vfprintf_unlocked_l(&f, loc, fmt, ap);
	*f._p = 0;
	return ret;
}

int
vsnprintf(char *str, size_t n, const char *fmt, va_list ap)
{
	return vsnprintf_l(str, n, _current_locale(), fmt, ap);
}

int
snprintf(char *str, size_t n, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(str, n, fmt, ap);
	va_end(ap);
	return ret;
}

int
snprintf_l(char *str, size_t n, locale_t loc, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf_l(str, n, loc, fmt, ap);
	va_end(ap);
	return ret;
}
