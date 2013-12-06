/*	$NetBSD: strerror_r.c,v 1.3 2013/08/19 13:03:12 joerg Exp $	*/

/*
 * Copyright (c) 1988 Regents of the University of California.
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
__RCSID("$NetBSD: strerror_r.c,v 1.3 2013/08/19 13:03:12 joerg Exp $");

#include "namespace.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef NLS
#include <limits.h>
#include <nl_types.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include "setlocale_local.h"
#endif

#include "extern.h"

__weak_alias(strerror_r, _strerror_r)

int
_strerror_lr(int num, char *buf, size_t buflen, locale_t loc)
{
#define	UPREFIX	"Unknown error: %u"
	unsigned int errnum = num;
	int retval = 0;
	size_t slen;
#ifdef NLS
	int saved_errno = errno;
	nl_catd catd;
	catd = catopen_l("libc", NL_CAT_LOCALE, loc);
#endif
	_DIAGASSERT(buf != NULL);

	if (errnum < (unsigned int) sys_nerr) {
#ifdef NLS
		slen = strlcpy(buf, catgets(catd, 1, (int)errnum,
		    sys_errlist[errnum]), buflen); 
#else
		slen = strlcpy(buf, sys_errlist[errnum], buflen); 
#endif
	} else {
#ifdef NLS
		slen = snprintf_l(buf, buflen, loc,
		    catgets(catd, 1, 0xffff, UPREFIX), errnum);
#else
		slen = snprintf(buf, buflen, UPREFIX, errnum);
#endif
		retval = EINVAL;
	}

	if (slen >= buflen)
		retval = ERANGE;

#ifdef NLS
	catclose(catd);
	errno = saved_errno;
#endif

	return retval;
}

int
strerror_r(int num, char *buf, size_t buflen)
{
#ifdef NLS
	return _strerror_lr(num, buf, buflen, _current_locale());
#else
	return _strerror_lr(num, buf, buflen, NULL);
#endif
}
