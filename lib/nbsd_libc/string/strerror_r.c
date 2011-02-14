/*	$NetBSD: strerror_r.c,v 1.2 2005/07/30 15:21:21 christos Exp $	*/

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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)strerror.c	5.6 (Berkeley) 5/4/91";
#else
__RCSID("$NetBSD: strerror_r.c,v 1.2 2005/07/30 15:21:21 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#ifdef NLS
#include <limits.h>
#include <nl_types.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "extern.h"

#ifdef _LIBC
# ifdef __weak_alias
__weak_alias(strerror_r, _strerror_r)
# endif
#endif

int
#ifdef _LIBC
_strerror_r(int num, char *buf, size_t buflen)
#else
strerror_r(int num, char *buf, size_t buflen)
#endif
{
#define	UPREFIX	"Unknown error: %u"
	unsigned int errnum = num;
	int retval = 0;
	size_t slen;
#ifdef NLS
	int saved_errno = errno;
	nl_catd catd;
	catd = catopen("libc", NL_CAT_LOCALE);
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
		slen = snprintf(buf, buflen, 
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
