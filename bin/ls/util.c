/*	$NetBSD: util.c,v 1.34 2011/08/29 14:44:21 joerg Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
#ifndef lint
#if 0
static char sccsid[] = "@(#)util.c	8.5 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: util.c,v 1.34 2011/08/29 14:44:21 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>
#include <wchar.h>
#include <wctype.h>

#include "ls.h"
#include "extern.h"

int
safe_print(const char *src)
{
	size_t len;
	char *name;
	int flags;

	flags = VIS_NL | VIS_OCTAL | VIS_WHITE;
	if (f_octal_escape)
		flags |= VIS_CSTYLE;

	len = strlen(src);
	if (len != 0 && SIZE_T_MAX/len <= 4) {
		errx(EXIT_FAILURE, "%s: name too long", src);
		/* NOTREACHED */
	}

	name = (char *)malloc(4*len+1);
	if (name != NULL) {
		len = strvis(name, src, flags);
		(void)printf("%s", name);
		free(name);
		return len;
	} else
		errx(EXIT_FAILURE, "out of memory!");
		/* NOTREACHED */
}

/*
 * The reasons why we don't use putwchar(wc) here are:
 * - If wc == L'\0', we need to restore the initial shift state, but
 *   the C language standard doesn't say that putwchar(L'\0') does.
 * - It isn't portable to mix a wide-oriented function (i.e. getwchar)
 *   with byte-oriented functions (printf et al.) in same FILE.
 */
static int
printwc(wchar_t wc, mbstate_t *pst)
{
	size_t size;
	char buf[MB_LEN_MAX];

	size = wcrtomb(buf, wc, pst);
	if (size == (size_t)-1) /* This shouldn't happen, but for sure */
		return 0;
	if (wc == L'\0') {
		/* The following condition must be always true, but for sure */
		if (size > 0 && buf[size - 1] == '\0')
			--size;
	}
	if (size > 0)
		fwrite(buf, 1, size, stdout);
	return wc == L'\0' ? 0 : wcwidth(wc);
}

int
printescaped(const char *src)
{
	int n = 0;
	mbstate_t src_state, stdout_state;
	/* The following +1 is to pass '\0' at the end of src to mbrtowc(). */
	const char *endptr = src + strlen(src) + 1;

	/*
	 * We have to reset src_state each time in this function, because
	 * the codeset of src pathname may not match with current locale.
	 * Note that if we pass NULL instead of src_state to mbrtowc(),
	 * there is no way to reset the state.
	 */
	memset(&src_state, 0, sizeof(src_state));
	memset(&stdout_state, 0, sizeof(stdout_state));
	while (src < endptr) {
		wchar_t wc;
		size_t rv, span = endptr - src;

#if 0
		/*
		 * XXX - we should fix libc instead.
		 * Theoretically this should work, but our current
		 * implementation of iso2022 module doesn't actually work
		 * as expected, if there are redundant escape sequences
		 * which exceed 32 bytes.
		 */
		if (span > MB_CUR_MAX)
			span = MB_CUR_MAX;
#endif
		rv = mbrtowc(&wc, src, span, &src_state);
		if (rv == 0) { /* assert(wc == L'\0'); */
			/* The following may output a shift sequence. */
			n += printwc(wc, &stdout_state);
			break;
		}
		if (rv == (size_t)-1) { /* probably errno == EILSEQ */
			n += printwc(L'?', &stdout_state);
			/* try to skip 1byte, because there is no better way */
			src++;
			memset(&src_state, 0, sizeof(src_state));
		} else if (rv == (size_t)-2) {
			if (span < MB_CUR_MAX) { /* incomplete char */
				n += printwc(L'?', &stdout_state);
				break;
			}
			src += span; /* a redundant shift sequence? */
		} else {
			n += printwc(iswprint(wc) ? wc : L'?', &stdout_state);
			src += rv;
		}
	}
	return n;
}
