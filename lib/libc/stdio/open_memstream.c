/*	$NetBSD: open_memstream.c,v 1.1 2014/10/13 00:40:36 christos Exp $	*/

/*-
 * Copyright (c) 2013 Advanced Computing Technologies LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__FBSDID("$FreeBSD: head/lib/libc/stdio/open_memstream.c 247411 2013-02-27 19:50:46Z jhb $");
#endif
__RCSID("$NetBSD: open_memstream.c,v 1.1 2014/10/13 00:40:36 christos Exp $");

#include "namespace.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define	OFF_MAX	LLONG_MAX

struct memstream {
	char **bufp;
	size_t *sizep;
	size_t len;
	size_t offset;
};

static int
memstream_grow(struct memstream *ms, off_t newoff)
{
	char *buf;
	size_t newsize;

	if (newoff < 0 || newoff >= SSIZE_MAX)
		newsize = SSIZE_MAX - 1;
	else
		newsize = newoff;
	if (newsize > ms->len) {
		buf = realloc(*ms->bufp, newsize + 1);
		if (buf != NULL) {
#ifdef DEBUG
			fprintf(stderr, "MS: %p growing from %zd to %zd\n",
			    ms, ms->len, newsize);
#endif
			memset(buf + ms->len + 1, 0, newsize - ms->len);
			*ms->bufp = buf;
			ms->len = newsize;
			return (1);
		}
		return (0);
	}
	return (1);
}

static void
memstream_update(struct memstream *ms)
{

	*ms->sizep = ms->len < ms->offset ? ms->len : ms->offset;
}

static ssize_t
memstream_write(void *cookie, const void *buf, size_t len)
{
	struct memstream *ms;
	size_t tocopy;
	off_t more;

	ms = cookie;
	more = ms->offset;
	more += len;
	if (!memstream_grow(ms, more))
		return (-1);
	tocopy = ms->len - ms->offset;
	if (len < tocopy)
		tocopy = len;
	memcpy(*ms->bufp + ms->offset, buf, tocopy);
	ms->offset += tocopy;
	memstream_update(ms);
#ifdef DEBUG
	fprintf(stderr, "MS: write(%p, %zu) = %zu\n", ms, len, tocopy);
#endif
	return (ssize_t)tocopy;
}

static off_t
memstream_seek(void *cookie, off_t pos, int whence)
{
	struct memstream *ms;
#ifdef DEBUG
	size_t old;
#endif

	ms = cookie;
#ifdef DEBUG
	old = ms->offset;
#endif
	switch (whence) {
	case SEEK_SET:
		/* _fseeko() checks for negative offsets. */
		assert(pos >= 0);
		ms->offset = pos;
		break;
	case SEEK_CUR:
		/* This is only called by _ftello(). */
		assert(pos == 0);
		break;
	case SEEK_END:
		if (pos < 0) {
			if (pos + (ssize_t)ms->len < 0) {
#ifdef DEBUG
				fprintf(stderr,
				    "MS: bad SEEK_END: pos %jd, len %zu\n",
				    (intmax_t)pos, ms->len);
#endif
				errno = EINVAL;
				return (-1);
			}
		} else {
			if (OFF_MAX - (ssize_t)ms->len < pos) {
#ifdef DEBUG
				fprintf(stderr,
				    "MS: bad SEEK_END: pos %jd, len %zu\n",
				    (intmax_t)pos, ms->len);
#endif
				errno = EOVERFLOW;
				return (-1);
			}
		}
		ms->offset = ms->len + pos;
		break;
	}
	memstream_update(ms);
#ifdef DEBUG
	fprintf(stderr, "MS: seek(%p, %jd, %d) %zu -> %zu\n", ms,
	    (intmax_t)pos, whence, old, ms->offset);
#endif
	return (ms->offset);
}

static int
memstream_close(void *cookie)
{

	free(cookie);
	return (0);
}

FILE *
open_memstream(char **bufp, size_t *sizep)
{
	struct memstream *ms;
	int save_errno;
	FILE *fp;

	if (bufp == NULL || sizep == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	*bufp = calloc(1, 1);
	if (*bufp == NULL)
		return (NULL);
	ms = malloc(sizeof(*ms));
	if (ms == NULL) {
		save_errno = errno;
		free(*bufp);
		*bufp = NULL;
		errno = save_errno;
		return (NULL);
	}
	ms->bufp = bufp;
	ms->sizep = sizep;
	ms->len = 0;
	ms->offset = 0;
	memstream_update(ms);
	fp = funopen2(ms, NULL, memstream_write, memstream_seek,
	    NULL, memstream_close);
	if (fp == NULL) {
		save_errno = errno;
		free(ms);
		free(*bufp);
		*bufp = NULL;
		errno = save_errno;
		return (NULL);
	}
	fwide(fp, -1);
	return (fp);
}
