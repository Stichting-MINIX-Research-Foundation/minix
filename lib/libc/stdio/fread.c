/*	$NetBSD: fread.c,v 1.22 2012/03/15 18:22:30 christos Exp $	*/

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
static char sccsid[] = "@(#)fread.c	8.2 (Berkeley) 12/11/93";
#else
__RCSID("$NetBSD: fread.c,v 1.22 2012/03/15 18:22:30 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "reentrant.h"
#include "local.h"

size_t
fread(void *buf, size_t size, size_t count, FILE *fp)
{
	size_t resid;
	char *p;
	int r;
	size_t total;

	_DIAGASSERT(fp != NULL);
	/*
	 * The ANSI standard requires a return value of 0 for a count
	 * or a size of 0.  Whilst ANSI imposes no such requirements on
	 * fwrite, the SUSv2 does.
	 */
	if ((resid = count * size) == 0)
		return 0;

	_DIAGASSERT(buf != NULL);

	FLOCKFILE(fp);
	if (fp->_r < 0)
		fp->_r = 0;
	total = resid;
	p = buf;
	while (resid > (size_t)(r = fp->_r)) {
		(void)memcpy(p, fp->_p, (size_t)r);
		fp->_p += r;
		/* fp->_r = 0 ... done in __srefill */
		p += r;
		resid -= r;
		if (__srefill(fp)) {
			/* no more input: return partial result */
			FUNLOCKFILE(fp);
			return (total - resid) / size;
		}
	}
	(void)memcpy(p, fp->_p, resid);

	_DIAGASSERT(__type_fit(int, fp->_r - resid));
	fp->_r -= (int)resid;
	fp->_p += resid;
	FUNLOCKFILE(fp);
	return count;
}
