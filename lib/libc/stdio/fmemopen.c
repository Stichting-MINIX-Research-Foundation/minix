/* $NetBSD: fmemopen.c,v 1.8 2012/03/29 14:27:33 christos Exp $ */

/*-
 * Copyright (c)2007, 2010 Takehiko NOZAKI,
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
 *
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fmemopen.c,v 1.8 2012/03/29 14:27:33 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "reentrant.h"
#include "local.h"

struct fmemopen_cookie {
	char *head, *tail, *cur, *eob;
};

static ssize_t
fmemopen_read(void *cookie, void *buf, size_t nbytes)
{
	struct fmemopen_cookie *p;
	char *s, *b = buf;

	_DIAGASSERT(cookie != NULL);
	_DIAGASSERT(buf != NULL && nbytes > 0);

	p = (struct fmemopen_cookie *)cookie;
	s = p->cur;
	do {
		if (p->cur == p->tail)
			break;
		*b++ = *p->cur++;
	} while (--nbytes > 0);

	return (ssize_t)(p->cur - s);
}

static ssize_t
fmemopen_write(void *cookie, const void *buf, size_t nbytes)
{
	struct fmemopen_cookie *p;
	char *s;
	const char *b = buf;

	_DIAGASSERT(cookie != NULL);
	_DIAGASSERT(buf != NULL && nbytes > 0);

	p = (struct fmemopen_cookie *)cookie;
	if (p->cur >= p->tail)
		return 0;
	s = p->cur;
	do {
		if (p->cur == p->tail - 1) {
			if (*b == '\0') {
				*p->cur++ = '\0';
				goto ok;
			}
			break;
		}
		*p->cur++ = *b++;
	} while (--nbytes > 0);
	*p->cur = '\0';
ok:
	if (p->cur > p->eob)
		p->eob = p->cur;

	return (ssize_t)(p->cur - s);
}

#ifdef notyet
static int
fmemopen_flush(void *cookie)
{
	struct fmemopen_cookie *p;

	_DIAGASSERT(cookie != NULL);

	p = (struct fmemopen_cookie *)cookie;
	if (p->cur >= p->tail)
		return -1;
	*p->cur = '\0';
	return 0;
}
#endif

static off_t
fmemopen_seek(void *cookie, off_t offset, int whence)
{
	struct fmemopen_cookie *p;
 
	_DIAGASSERT(cookie != NULL);

	p = (struct fmemopen_cookie *)cookie;
	switch (whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += p->cur - p->head;
		break;
	case SEEK_END:
		offset += p->eob - p->head;
		break;
	default:
		errno = EINVAL;
		goto error;
	}
	if (offset >= (off_t)0 && offset <= p->tail - p->head) {
		p->cur = p->head + (ptrdiff_t)offset;
		return (off_t)(p->cur - p->head);
	}
error:
	return (off_t)-1;
}

static int
fmemopen_close0(void *cookie)
{
	_DIAGASSERT(cookie != NULL);

	free(cookie);

	return 0;
}

static int
fmemopen_close1(void *cookie)
{
	struct fmemopen_cookie *p;

	_DIAGASSERT(cookie != NULL);

	p = (struct fmemopen_cookie *)cookie;
	free(p->head);
	free(p);

	return 0;
}


FILE *
fmemopen(void * __restrict buf, size_t size, const char * __restrict mode)
{
	int flags, oflags;
	FILE *fp;
	struct fmemopen_cookie *cookie;

	if (size < (size_t)1)
		goto invalid;

	flags = __sflags(mode, &oflags);
	if (flags == 0)
		return NULL;

	if ((oflags & O_RDWR) == 0 && buf == NULL)
		goto invalid;

	fp = __sfp();
	if (fp == NULL)
		return NULL;
	fp->_file = -1;

	cookie = malloc(sizeof(*cookie));
	if (cookie == NULL)
		goto release;

	if (buf == NULL) {
		cookie->head = malloc(size);
		if (cookie->head == NULL) {
			free(cookie);
			goto release;
		}
		*cookie->head = '\0';
		fp->_close = fmemopen_close1;
	} else {
		cookie->head = (char *)buf;
		if (oflags & O_TRUNC)
			*cookie->head = '\0';
		fp->_close = fmemopen_close0;
	}

	cookie->tail = cookie->head + size;
	cookie->eob  = cookie->head;
	do {
		if (*cookie->eob == '\0')
			break;
		++cookie->eob;
	} while (--size > 0);

	cookie->cur = (oflags & O_APPEND) ? cookie->eob : cookie->head;

	fp->_flags  = flags;
	fp->_write  = (flags & __SRD) ? NULL : fmemopen_write;
	fp->_read   = (flags & __SWR) ? NULL : fmemopen_read;
	fp->_seek   = fmemopen_seek;
#ifdef notyet
	fp->_flush  = fmemopen_flush;
#endif
	fp->_cookie = (void *)cookie;

	return fp;

invalid:
	errno = EINVAL;
	return NULL;

release:
	fp->_flags = 0;
	return NULL;
}
