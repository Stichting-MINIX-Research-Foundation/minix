/*	$NetBSD: _strtoi.h,v 1.2 2015/01/18 17:55:22 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * Original version ID:
 * NetBSD: src/lib/libc/locale/_wcstoul.h,v 1.2 2003/08/07 16:43:03 agc Exp
 *
 * Created by Kamil Rytarowski, based on ID:
 * NetBSD: src/common/lib/libc/stdlib/_strtoul.h,v 1.7 2013/05/17 12:55:56 joerg Exp
 */

/*
 * function template for strtoi and strtou
 *
 * parameters:
 *	_FUNCNAME    : function name
 *      __TYPE       : return and range limits type
 *      __WRAPPED    : wrapped function, strtoimax or strtoumax
 */

#define __WRAPPED_L_(x) x ## _l
#define __WRAPPED_L__(x) __WRAPPED_L_(x)
#define __WRAPPED_L __WRAPPED_L__(__WRAPPED)

#if defined(_KERNEL) || defined(_STANDALONE) || \
    defined(HAVE_NBTOOL_CONFIG_H) || defined(BCS_ONLY)
__TYPE
_FUNCNAME(const char * __restrict nptr, char ** __restrict endptr, int base,
          __TYPE lo, __TYPE hi, int * rstatus)
#else
#include <locale.h>
#include "setlocale_local.h"
#define INT_FUNCNAME_(pre, name, post)	pre ## name ## post
#define INT_FUNCNAME(pre, name, post)	INT_FUNCNAME_(pre, name, post)

static __TYPE
INT_FUNCNAME(_int_, _FUNCNAME, _l)(const char * __restrict nptr,
    char ** __restrict endptr, int base,
    __TYPE lo, __TYPE hi, int * rstatus, locale_t loc)
#endif
{
#if !defined(_KERNEL) && !defined(_STANDALONE)
	int serrno;
#endif
	__TYPE im;
	char *ep;
	int rep;

	_DIAGASSERT(hi >= lo);

	_DIAGASSERT(nptr != NULL);
	/* endptr may be NULL */

	if (endptr == NULL)
		endptr = &ep;

	if (rstatus == NULL)
		rstatus = &rep;

#if !defined(_KERNEL) && !defined(_STANDALONE)
	serrno = errno;
	errno = 0;
#endif

#if defined(_KERNEL) || defined(_STANDALONE) || \
    defined(HAVE_NBTOOL_CONFIG_H) || defined(BCS_ONLY)
	im = __WRAPPED(nptr, endptr, base);
#else
	im = __WRAPPED_L(nptr, endptr, base, loc);
#endif

#if !defined(_KERNEL) && !defined(_STANDALONE)
	*rstatus = errno;
	errno = serrno;
#endif

	if (*rstatus == 0) {
		/* No digits were found */
		if (nptr == *endptr)
			*rstatus = ECANCELED;
		/* There are further characters after number */
		else if (**endptr != '\0')
			*rstatus = ENOTSUP;
	}

	if (im < lo) {
		if (*rstatus == 0)
			*rstatus = ERANGE;
		return lo;
	}
	if (im > hi) {
		if (*rstatus == 0)
			*rstatus = ERANGE;
		return hi;
	}

	return im;
}

#if !defined(_KERNEL) && !defined(_STANDALONE) && \
    !defined(HAVE_NBTOOL_CONFIG_H) && !defined(BCS_ONLY)
__TYPE
_FUNCNAME(const char * __restrict nptr, char ** __restrict endptr, int base,
    __TYPE lo, __TYPE hi, int * rstatus)
{
	return INT_FUNCNAME(_int_, _FUNCNAME, _l)(nptr, endptr, base, lo, hi,
	    rstatus, _current_locale());
}

__TYPE
INT_FUNCNAME(, _FUNCNAME, _l)(const char * __restrict nptr,
    char ** __restrict endptr, int base,
    __TYPE lo, __TYPE hi, int * rstatus, locale_t loc)
{
	return INT_FUNCNAME(_int_, _FUNCNAME, _l)(nptr, endptr, base, lo, hi,
	    rstatus, loc);
}
#endif
