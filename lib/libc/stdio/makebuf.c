/*	$NetBSD: makebuf.c,v 1.18 2015/07/15 19:08:43 christos Exp $	*/

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
static char sccsid[] = "@(#)makebuf.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: makebuf.c,v 1.18 2015/07/15 19:08:43 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <ctype.h>
#include "reentrant.h"
#include "local.h"

/*
 * Override the file buffering based on the environment setting STDBUF%d
 * (for the specific file descriptor) and STDBUF (for all descriptors).
 * the setting is ULB<num> standing for "Unbuffered", "Linebuffered",
 * and Fullybuffered", and <num> is a value from 0 to 1M.
 */
static int
__senvbuf(FILE *fp, size_t *size, int *couldbetty)
{
	char evb[64], *evp;
	int flags, e;
	intmax_t s;

	flags = 0;
	if (snprintf(evb, sizeof(evb), "STDBUF%d", fp->_file) < 0)
		return flags;

	if ((evp = getenv(evb)) == NULL && (evp = getenv("STDBUF")) == NULL)
		return flags;

	switch (*evp) {
	case 'u':
	case 'U':
		evp++;
		flags |= __SNBF;
		break;
	case 'l':
	case 'L':
		evp++;
		flags |= __SLBF;
		break;
	case 'f':
	case 'F':
		evp++;
		*couldbetty = 0;
		break;
	}

	if (!isdigit((unsigned char)*evp))
		return flags;

	s = strtoi(evp, NULL, 0, 0, 1024 * 1024, &e);
	if (e != 0)
		return flags;

	*size = (size_t)s;
	if (*size == 0)
		return __SNBF;

	return flags;
}

/*
 * Allocate a file buffer, or switch to unbuffered I/O.
 * Per the ANSI C standard, ALL tty devices default to line buffered.
 *
 * As a side effect, we set __SOPT or __SNPT (en/dis-able fseek
 * optimisation) right after the fstat() that finds the buffer size.
 */
void
__smakebuf(FILE *fp)
{
	void *p;
	int flags;
	size_t size;
	int couldbetty;

	_DIAGASSERT(fp != NULL);

	if (fp->_flags & __SNBF)
		goto unbuf;

	flags = __swhatbuf(fp, &size, &couldbetty);

	if ((fp->_flags & (__SLBF|__SNBF|__SMBF)) == 0
	    && fp->_cookie == fp && fp->_file >= 0) {
		flags |= __senvbuf(fp, &size, &couldbetty);
		if (flags & __SNBF)
			goto unbuf;
	}

	if ((p = malloc(size)) == NULL)
		goto unbuf;

	__cleanup = _cleanup;
	flags |= __SMBF;
	fp->_bf._base = fp->_p = p;
	_DIAGASSERT(__type_fit(int, size));
	fp->_bf._size = (int)size;
	if (couldbetty && isatty(__sfileno(fp)))
		flags |= __SLBF;
	fp->_flags |= flags;
	return;
unbuf:
	fp->_flags |= __SNBF;
	fp->_bf._base = fp->_p = fp->_nbuf;
	fp->_bf._size = 1;
}

/*
 * Internal routine to determine `proper' buffering for a file.
 */
int
__swhatbuf(FILE *fp, size_t *bufsize, int *couldbetty)
{
	struct stat st;

	_DIAGASSERT(fp != NULL);
	_DIAGASSERT(bufsize != NULL);
	_DIAGASSERT(couldbetty != NULL);

	if (__sfileno(fp) == -1 || fstat(__sfileno(fp), &st) < 0) {
		*couldbetty = 0;
		*bufsize = BUFSIZ;
		return __SNPT;
	}

	/* could be a tty iff it is a character device */
	*couldbetty = S_ISCHR(st.st_mode);
	if (st.st_blksize == 0) {
		*bufsize = BUFSIZ;
		return __SNPT;
	}

	/*
	 * Optimise fseek() only if it is a regular file.  (The test for
	 * __sseek is mainly paranoia.)  It is safe to set _blksize
	 * unconditionally; it will only be used if __SOPT is also set.
	 */
	*bufsize = st.st_blksize;
	fp->_blksize = st.st_blksize;
	return (st.st_mode & S_IFMT) == S_IFREG && fp->_seek == __sseek ?
	    __SOPT : __SNPT;
}
