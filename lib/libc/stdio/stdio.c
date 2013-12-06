/*	$NetBSD: stdio.c,v 1.22 2013/05/19 17:07:04 christos Exp $	*/

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
static char sccsid[] = "@(#)stdio.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: stdio.c,v 1.22 2013/05/19 17:07:04 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "reentrant.h"
#include "local.h"

/*
 * Small standard I/O/seek/close functions.
 * These maintain the `known seek offset' for seek optimisation.
 */
ssize_t
__sread(void *cookie, void *buf, size_t n)
{
	FILE *fp = cookie;
	ssize_t ret;
	
	_DIAGASSERT(cookie != NULL);
	_DIAGASSERT(cookie == fp->_cookie);
	_DIAGASSERT(buf != NULL);

	ret = read(__sfileno(fp), buf, n);
	/* if the read succeeded, update the current offset */
	if (ret >= 0)
		fp->_offset += ret;
	else
		fp->_flags &= ~__SOFF;	/* paranoia */

	return ret;
}

ssize_t
__swrite(void *cookie, const void *buf, size_t n)
{
	FILE *fp = cookie;

	_DIAGASSERT(cookie != NULL);
	_DIAGASSERT(cookie == fp->_cookie);
	_DIAGASSERT(buf != NULL);

	if (fp->_flags & __SAPP)
		if (lseek(__sfileno(fp), (off_t)0, SEEK_END) == (off_t)-1) {
			if (errno == ESPIPE)            /* if unseekable, OK, */
				fp->_flags &= ~__SAPP;  /* all writes append. */
			else
				return -1;
		}
	fp->_flags &= ~__SOFF;	/* in case FAPPEND mode is set */
	return write(__sfileno(fp), buf, n);
}

off_t
__sseek(void *cookie, off_t offset, int whence)
{
	FILE *fp = cookie;
	off_t ret;

	_DIAGASSERT(cookie != NULL);
	_DIAGASSERT(cookie == fp->_cookie);
	
	ret = lseek(__sfileno(fp), offset, whence);
	if (ret == (off_t)-1L)
		fp->_flags &= ~__SOFF;
	else {
		fp->_flags |= __SOFF;
		fp->_offset = ret;
	}
	return ret;
}

int
__sclose(void *cookie)
{
	FILE *fp = cookie;

	_DIAGASSERT(cookie != NULL);
	_DIAGASSERT(cookie == fp->_cookie);

	return close(__sfileno(fp));
}
