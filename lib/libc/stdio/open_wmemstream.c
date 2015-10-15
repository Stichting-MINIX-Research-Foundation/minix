/*	$NetBSD: open_wmemstream.c,v 1.1 2014/10/13 00:40:36 christos Exp $	*/

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
__FBSDID("$FreeBSD: head/lib/libc/stdio/open_wmemstream.c 247411 2013-02-27 19:50:46Z jhb $");
#endif
__RCSID("$NetBSD: open_wmemstream.c,v 1.1 2014/10/13 00:40:36 christos Exp $");

#include "namespace.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define	OFF_MAX LLONG_MAX

struct wmemstream {
	wchar_t **bufp;
	size_t *sizep;
	size_t len;
	size_t offset;
	mbstate_t mbstate;
};

static int
wmemstream_grow(struct wmemstream *ms, size_t newoff)
{
	wchar_t *buf;
	size_t newsize;

	if (newoff >= (off_t)(SSIZE_MAX / sizeof(wchar_t)))
		newsize = SSIZE_MAX / sizeof(wchar_t) - 1;
	else
		newsize = newoff;
	if (newsize > ms->len) {
		buf = realloc(*ms->bufp, (newsize + 1) * sizeof(wchar_t));
		if (buf != NULL) {
#ifdef DEBUG
			fprintf(stderr, "WMS: %p growing from %zu to %zu\n",
			    ms, ms->len, newsize);
#endif
			wmemset(buf + ms->len + 1, 0, newsize - ms->len);
			*ms->bufp = buf;
			ms->len = newsize;
			return (1);
		}
		return (0);
	}
	return (1);
}

static void
wmemstream_update(struct wmemstream *ms)
{

	*ms->sizep = ms->len < ms->offset ? ms->len : ms->offset;
}

/*
 * Based on a starting multibyte state and an input buffer, determine
 * how many wchar_t's would be output.  This doesn't use mbsnrtowcs()
 * so that it can handle embedded null characters.
 */
static ssize_t
wbuflen(const mbstate_t *state, const char *buf, size_t len)
{
	mbstate_t lenstate;
	size_t charlen, count;

	count = 0;
	lenstate = *state;
	while (len > 0) {
		charlen = mbrlen(buf, len, &lenstate);
		if (charlen == (size_t)-1)
			return (-1);
		if (charlen == (size_t)-2)
			break;
		if (charlen == 0)
			/* XXX: Not sure how else to handle this. */
			charlen = 1;
		len -= charlen;
		buf += charlen;
		count++;
	}
	return (count);
}

static ssize_t
wmemstream_write(void *cookie, const void *buf, size_t len)
{
	struct wmemstream *ms;
	ssize_t consumed, wlen;
	size_t charlen;

	ms = cookie;
	wlen = wbuflen(&ms->mbstate, buf, len);
	if (wlen < 0) {
		errno = EILSEQ;
		return (-1);
	}
	if (!wmemstream_grow(ms, ms->offset + wlen))
		return (-1);

	/*
	 * This copies characters one at a time rather than using
	 * mbsnrtowcs() so it can properly handle embedded null
	 * characters.
	 */
	consumed = 0;
	while (len > 0 && ms->offset < ms->len) {
		charlen = mbrtowc(*ms->bufp + ms->offset, buf, len,
		    &ms->mbstate);
		if (charlen == (size_t)-1) {
			if (consumed == 0) {
				errno = EILSEQ;
				return (-1);
			}
			/* Treat it as a successful short write. */
			break;
		}
		if (charlen == 0)
			/* XXX: Not sure how else to handle this. */
			charlen = 1;
		if (charlen == (size_t)-2) {
			consumed += len;
			len = 0;
		} else {
			consumed += charlen;
			buf = (const char *)buf + charlen;
			len -= charlen;
			ms->offset++;
		}
	}
	wmemstream_update(ms);
#ifdef DEBUG
	fprintf(stderr, "WMS: write(%p, %zu) = %zd\n", ms, len, consumed);
#endif
	return (consumed);
}

static off_t
wmemstream_seek(void *cookie, off_t pos, int whence)
{
	struct wmemstream *ms;
	size_t old;

	ms = cookie;
	old = ms->offset;
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
				    "WMS: bad SEEK_END: pos %jd, len %zd\n",
				    (intmax_t)pos, ms->len);
#endif
				errno = EINVAL;
				return (-1);
			}
		} else {
			if (OFF_MAX - ms->len < (size_t)pos) {
#ifdef DEBUG
				fprintf(stderr,
				    "WMS: bad SEEK_END: pos %jd, len %zd\n",
				    (intmax_t)pos, ms->len);
#endif
				errno = EOVERFLOW;
				return (-1);
			}
		}
		ms->offset = ms->len + pos;
		break;
	}
	/* Reset the multibyte state if a seek changes the position. */
	if (ms->offset != old)
		memset(&ms->mbstate, 0, sizeof(ms->mbstate));
	wmemstream_update(ms);
#ifdef DEBUG
	fprintf(stderr, "WMS: seek(%p, %jd, %d) %jd -> %jd\n", ms,
	    (intmax_t)pos, whence, (intmax_t)old, (intmax_t)ms->offset);
#endif
	return (ms->offset);
}

static int
wmemstream_close(void *cookie)
{

	free(cookie);
	return (0);
}

FILE *
open_wmemstream(wchar_t **bufp, size_t *sizep)
{
	struct wmemstream *ms;
	int save_errno;
	FILE *fp;

	if (bufp == NULL || sizep == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	*bufp = calloc(1, sizeof(wchar_t));
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
	memset(&ms->mbstate, 0, sizeof(mbstate_t));
	wmemstream_update(ms);
	fp = funopen2(ms, NULL, wmemstream_write, wmemstream_seek,
	    NULL, wmemstream_close);
	if (fp == NULL) {
		save_errno = errno;
		free(ms);
		free(*bufp);
		*bufp = NULL;
		errno = save_errno;
		return (NULL);
	}
	fwide(fp, 1);
	return (fp);
}
