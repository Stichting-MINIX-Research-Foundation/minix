/*	$NetBSD: fgets.c,v 1.28 2012/03/15 18:22:30 christos Exp $	*/

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
static char sccsid[] = "@(#)fgets.c	8.2 (Berkeley) 12/22/93";
#else
__RCSID("$NetBSD: fgets.c,v 1.28 2012/03/15 18:22:30 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "reentrant.h"
#include "local.h"
#ifdef _FORTIFY_SOURCE
#undef fgets
#endif

/*
 * Read at most n-1 characters from the given file.
 * Stop when a newline has been read, or the count runs out.
 * Return first argument, or NULL if no characters were read.
 */
char *
fgets(char *buf, int n, FILE *fp)
{
	int len;
	char *s;
	unsigned char *p, *t;

	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(fp != NULL);

	FLOCKFILE(fp);
	_SET_ORIENTATION(fp, -1);
	s = buf;
	n--;			/* leave space for NUL */
	do {
		/*
		 * If the buffer is empty, refill it.
		 */
		if (fp->_r <= 0) {
			if (__srefill(fp)) {
				/* EOF/error: stop with partial or no line */
				if (s == buf) {
					FUNLOCKFILE(fp);
					return (NULL);
				}
				break;
			}
		}
		len = fp->_r;
		p = fp->_p;

		/*
		 * Scan through at most n bytes of the current buffer,
		 * looking for '\n'.  If found, copy up to and including
		 * newline, and stop.  Otherwise, copy entire chunk
		 * and loop.
		 */
		if (len > n) {
			if (n < 0) {
				/*
				 * Caller's length <= 0
				 * We can't write into the buffer, so cannot
				 * return a string, so must return NULL.
				 * Set errno and __SERR so it is consistent.
				 * TOG gives no indication of what to do here!
				 */
				errno = EINVAL;
				fp->_flags |= __SERR;
				FUNLOCKFILE(fp);
				return NULL;
			}
			len = n;
		}
		t = memchr(p, '\n', (size_t)len);
		if (t != NULL) {
			len = (int)(++t - p);
			fp->_r -= len;
			fp->_p = t;
			(void)memcpy(s, p, (size_t)len);
			s[len] = 0;
			FUNLOCKFILE(fp);
			return buf;
		}
		fp->_r -= len;
		fp->_p += len;
		(void)memcpy(s, p, (size_t)len);
		s += len;
		n -= len;
	} while (n != 0);
	*s = 0;
	FUNLOCKFILE(fp);
	return buf;
}
