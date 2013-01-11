/*	$NetBSD: vswscanf.c,v 1.9 2012/03/27 15:05:42 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at UUNET Technologies, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)vsscanf.c	8.1 (Berkeley) 6/4/93";
__FBSDID("$FreeBSD: src/lib/libc/stdio/vswscanf.c,v 1.3 2004/04/07 09:55:05 tjr Exp $");
#else
__RCSID("$NetBSD: vswscanf.c,v 1.9 2012/03/27 15:05:42 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "reentrant.h"
#include "local.h"

static ssize_t
/*ARGSUSED*/
eofread(void *cookie, void *buf, size_t len)
{

	return 0;
}

int
vswscanf(const wchar_t * __restrict str, const wchar_t * __restrict fmt,
    va_list ap)
{
	static const mbstate_t initial;
	mbstate_t mbs;
	FILE f;
	char *mbstr;
	size_t mlen;
	int r;
	const wchar_t *rstr = str;
	struct __sfileext fext;

	/*
	 * XXX Convert the wide character string to multibyte, which
	 * __vfwscanf() will convert back to wide characters.
	 */
	if ((mbstr = malloc(wcslen(str) * MB_CUR_MAX + 1)) == NULL)
		return EOF;
	mbs = initial;
	if ((mlen = wcsrtombs(mbstr, &rstr, SIZE_T_MAX, &mbs)) == (size_t)-1) {
		free(mbstr);
		return EOF;
	}
	_FILEEXT_SETUP(&f, &fext);
	(void)memset(WCIO_GET(&f), 0, sizeof(struct wchar_io_data));
	f._file = -1;
	f._flags = __SRD;
	f._bf._base = f._p = (unsigned char *)mbstr;
	_DIAGASSERT(__type_fit(int, mlen));
	f._bf._size = f._r = (int)mlen;
	f._read = eofread;
	_UB(&f)._base = NULL;
	r = __vfwscanf_unlocked(&f, fmt, ap);
	free(mbstr);

	return r;
}
