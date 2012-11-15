/*	$NetBSD: funopen.c,v 1.14 2012/03/28 15:21:11 christos Exp $	*/

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
static char sccsid[] = "@(#)funopen.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: funopen.c,v 1.14 2012/03/28 15:21:11 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include "reentrant.h"
#include "local.h"

FILE *
funopen2(const void *cookie,
    ssize_t (*readfn)(void *, void *, size_t),
    ssize_t (*writefn)(void *, const void *, size_t),
    off_t (*seekfn)(void *, off_t, int),
    int (*flushfn)(void *),
    int (*closefn)(void *))
{
	FILE *fp;
	int flags;

	if (readfn == NULL) {
		if (writefn == NULL) {		/* illegal */
			errno = EINVAL;
			return NULL;
		} else
			flags = __SWR;		/* write only */
	} else {
		if (writefn == NULL)
			flags = __SRD;		/* read only */
		else
			flags = __SRW;		/* read-write */
	}
	if ((fp = __sfp()) == NULL)
		return NULL;
	fp->_flags = flags;
	fp->_file = -1;
	fp->_cookie = __UNCONST(cookie);
	fp->_read = readfn;
	fp->_write = writefn;
	fp->_seek = seekfn;
	fp->_flush = flushfn;
	fp->_close = closefn;
	return fp;
}

typedef struct {
	void *cookie;
	int (*readfn)(void *, char *, int);
	int (*writefn)(void *, const char *, int);
	off_t (*seekfn)(void *, off_t, int);
	int (*closefn)(void *);
} dookie_t;

static ssize_t
creadfn(void *dookie, void *buf, size_t len)
{
	dookie_t *d = dookie;
	if (len > INT_MAX)
		len = INT_MAX;
	return (*d->readfn)(d->cookie, buf, (int)len);
}

static ssize_t
cwritefn(void *dookie, const void *buf, size_t len)
{
	dookie_t *d = dookie;
	ssize_t nr;
	size_t l = len;
	const char *b = buf;

	while (l) {
		size_t nw = l > INT_MAX ? INT_MAX : l;
		nr = (*d->writefn)(d->cookie, buf, (int)nw);
		if (nr == -1) {
			if (len == l)
				return -1;
			else
				return len - l;
		}
		b += nr;
		l -= nr;
	}
	return len;
}

static off_t
cseekfn(void *dookie, off_t off, int whence)
{
	dookie_t *d = dookie;
	return (*d->seekfn)(d->cookie, off, whence);
}

static int
cclosefn(void *dookie)
{
	dookie_t *d = dookie;
	void *c = d->cookie;
	int (*cf)(void *) = d->closefn;
	free(dookie);
	return (*cf)(c);
}

FILE *
funopen(const void *cookie,
    int (*readfn)(void *, char *, int),
    int (*writefn)(void *, const char *, int),
    off_t (*seekfn)(void *, off_t, int),
    int (*closefn)(void *))
{
	dookie_t *d;
	FILE *fp;

	if ((d = malloc(sizeof(*d))) == NULL)
		return NULL;

	d->cookie = __UNCONST(cookie);
	d->readfn = readfn;
	d->writefn = writefn;
	d->seekfn = seekfn;
	d->closefn = closefn;
	fp = funopen2(d,
	    d->readfn ? creadfn : NULL,
	    d->writefn ? cwritefn : NULL,
	    d->seekfn ? cseekfn : NULL,
	    NULL,
	    d->closefn ? cclosefn : NULL);
	if (fp != NULL)
		return fp;
	free(d);
	return NULL;
 }
