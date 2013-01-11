/* $NetBSD: getdelim.c,v 1.13 2011/07/22 23:12:30 joerg Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: getdelim.c,v 1.13 2011/07/22 23:12:30 joerg Exp $");

#include "namespace.h"

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reentrant.h"
#include "local.h"

#ifdef __weak_alias
__weak_alias(getdelim, _getdelim)
#endif

/* Minimum buffer size we create.
 * This should allow config files to fit into our power of 2 buffer growth
 * without the need for a realloc. */
#define MINBUF	128

ssize_t
__getdelim(char **__restrict buf, size_t *__restrict buflen,
    int sep, FILE *__restrict fp)
{
	unsigned char *p;
	size_t len, newlen, off;
	char *newb;

	_DIAGASSERT(fp != NULL);

	if (buf == NULL || buflen == NULL) {
		errno = EINVAL;
		goto error;
	}

	/* If buf is NULL, we have to assume a size of zero */
	if (*buf == NULL)
		*buflen = 0;

	_SET_ORIENTATION(fp, -1);
	off = 0;
	do {
		/* If the input buffer is empty, refill it */
		if (fp->_r <= 0 && __srefill(fp)) {
			if (__sferror(fp))
				goto error;
			/* No error, so EOF. */
			break;
		}

		/* Scan through looking for the separator */
		p = memchr(fp->_p, sep, (size_t)fp->_r);
		if (p == NULL)
			len = fp->_r;
		else
			len = (p - fp->_p) + 1;

		newlen = off + len;
		/* Ensure we can handle it */
		if (newlen < off || newlen > SSIZE_MAX) {
			errno = EOVERFLOW;
			goto error;
		}
		newlen++; /* reserve space for the NULL terminator */
		if (newlen > *buflen) {
			if (newlen < MINBUF)
				newlen = MINBUF;
			if (!powerof2(newlen)) {
				/* Grow the buffer to the next power of 2 */
				newlen--;
				newlen |= newlen >> 1;
				newlen |= newlen >> 2;
				newlen |= newlen >> 4;
				newlen |= newlen >> 8;
				newlen |= newlen >> 16;
#if SIZE_T_MAX > 0xffffffffU
				newlen |= newlen >> 32;
#endif
				newlen++;
			}

			newb = realloc(*buf, newlen);
			if (newb == NULL)
				goto error;
			*buf = newb;
			*buflen = newlen;
		}

		(void)memcpy((*buf + off), fp->_p, len);
		/* Safe, len is never greater than what fp->_r can fit. */
		fp->_r -= (int)len;
		fp->_p += (int)len;
		off += len;
	} while (p == NULL);

	/* POSIX demands we return -1 on EOF. */
	if (off == 0) 
		return -1;

	if (*buf != NULL)
		*(*buf + off) = '\0';
	return off;

error:
	fp->_flags |= __SERR;
	return -1;
}

ssize_t
getdelim(char **__restrict buf, size_t *__restrict buflen,
    int sep, FILE *__restrict fp)
{
	ssize_t n;

	FLOCKFILE(fp);
	n = __getdelim(buf, buflen, sep, fp);
	FUNLOCKFILE(fp);
	return n;
}
