/*	$NetBSD: refill.c,v 1.16 2012/03/27 15:05:42 christos Exp $	*/

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
static char sccsid[] = "@(#)refill.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: refill.c,v 1.16 2012/03/27 15:05:42 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "reentrant.h"
#include "local.h"

#ifdef _REENTRANT
extern rwlock_t __sfp_lock;
#endif

static int
lflush(FILE *fp)
{

	_DIAGASSERT(fp != NULL);

	if ((fp->_flags & (__SLBF|__SWR)) == (__SLBF|__SWR))
		return __sflush(fp);
	return 0;
}

/*
 * Refill a stdio buffer.
 * Return EOF on eof or error, 0 otherwise.
 */
int
__srefill(FILE *fp)
{

	_DIAGASSERT(fp != NULL);

	/* make sure stdio is set up */
	if (!__sdidinit)
		__sinit();

	fp->_r = 0;		/* largely a convenience for callers */

	/* SysV does not make this test; take it out for compatibility */
	if (fp->_flags & __SEOF)
		return EOF;

	/* if not already reading, have to be reading and writing */
	if ((fp->_flags & __SRD) == 0) {
		if ((fp->_flags & __SRW) == 0) {
			errno = EBADF;
			fp->_flags |= __SERR;
			return EOF;
		}
		/* switch to reading */
		if (fp->_flags & __SWR) {
			if (__sflush(fp))
				return EOF;
			fp->_flags &= ~__SWR;
			fp->_w = 0;
			fp->_lbfsize = 0;
		}
		fp->_flags |= __SRD;
	} else {
		/*
		 * We were reading.  If there is an ungetc buffer,
		 * we must have been reading from that.  Drop it,
		 * restoring the previous buffer (if any).  If there
		 * is anything in that buffer, return.
		 */
		if (HASUB(fp)) {
			FREEUB(fp);
			if ((fp->_r = fp->_ur) != 0) {
				fp->_p = fp->_up;
				return 0;
			}
		}
	}

	if (fp->_bf._base == NULL)
		__smakebuf(fp);

	/*
	 * Before reading from a line buffered or unbuffered file,
	 * flush all line buffered output files, per the ANSI C
	 * standard.
	 */
	if (fp->_flags & (__SLBF|__SNBF)) {
		rwlock_rdlock(&__sfp_lock);
		(void) _fwalk(lflush);
		rwlock_unlock(&__sfp_lock);
	}
	fp->_p = fp->_bf._base;
	fp->_r = (int)(*fp->_read)(fp->_cookie, (char *)fp->_p,
	    (size_t)fp->_bf._size);
	fp->_flags &= ~__SMOD;	/* buffer contents are again pristine */
	if (fp->_r <= 0) {
		if (fp->_r == 0)
			fp->_flags |= __SEOF;
		else {
			fp->_r = 0;
			fp->_flags |= __SERR;
		}
		return EOF;
	}
	return 0;
}
