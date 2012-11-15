/*	$NetBSD: fflush.c,v 1.18 2012/03/27 15:05:42 christos Exp $	*/

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
static char sccsid[] = "@(#)fflush.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: fflush.c,v 1.18 2012/03/27 15:05:42 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "reentrant.h"
#include "local.h"

#ifdef _REENTRANT
extern rwlock_t __sfp_lock;
#endif

/* Flush a single file, or (if fp is NULL) all files.  */
int
fflush(FILE *fp)
{
	int r;

	if (fp == NULL) {
		rwlock_rdlock(&__sfp_lock);
		r = _fwalk(__sflush);
		rwlock_unlock(&__sfp_lock);
		return r;
	}

	FLOCKFILE(fp);
	if ((fp->_flags & (__SWR | __SRW)) == 0) {
		errno = EBADF;
		r = EOF;
	} else {
		r = __sflush(fp);
	}
	FUNLOCKFILE(fp);
	return r;
}

int
__sflush(FILE *fp)
{
	unsigned char *p;
	size_t n;
	ssize_t t;

	_DIAGASSERT(fp != NULL);

	t = fp->_flags;
	if ((t & __SWR) == 0)
		return 0;

	if ((p = fp->_bf._base) == NULL)
		return 0;

	ptrdiff_t tp = fp->_p - p;
	_DIAGASSERT(__type_fit(ssize_t, tp));
	n = (ssize_t)tp;	/* write this much */

	/*
	 * Set these immediately to avoid problems with longjmp and to allow
	 * exchange buffering (via setvbuf) in user write function.
	 */
	fp->_p = p;
	fp->_w = t & (__SLBF|__SNBF) ? 0 : fp->_bf._size;

	for (; n > 0; n -= t, p += t) {
		t = (*fp->_write)(fp->_cookie, (char *)p, n);
		if (t <= 0) {
			fp->_flags |= __SERR;
			return EOF;
		}
	}
	if (fp->_flush)
		return (*fp->_flush)(fp->_cookie);
	return 0;
}
