/*	$NetBSD: vdprintf.c,v 1.1 2010/09/06 14:52:55 christos Exp $	*/

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
__RCSID("$NetBSD: vdprintf.c,v 1.1 2010/09/06 14:52:55 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

#include "reentrant.h"
#include "local.h"

#ifdef __weak_alias
__weak_alias(vdprintf,_vdprintf)
#endif

int
vdprintf(int fd, const char * __restrict fmt, _BSD_VA_LIST_ ap)
{
	FILE f;
	struct __sfileext fext;
	unsigned char buf[BUFSIZ];
	int ret, fdflags, tmp;

	_DIAGASSERT(fd != -1);

	/*
	 * File descriptors are a full int, but _file is only a short.
	 * If we get a valid file descriptor that is greater or equal to
	 * USHRT_MAX, then the fd will get sign-extended into an
	 * invalid file descriptor.  Handle this case by failing the
	 * open. (We treat the short as unsigned, and special-case -1).
	 */
	if (fd >= USHRT_MAX) {
		errno = EMFILE;
		return EOF;
	}

	if ((fdflags = fcntl(fd, F_GETFL, 0)) == -1)
		return EOF;

	tmp = fdflags & O_ACCMODE;
	if (tmp != O_RDWR && tmp != O_WRONLY) {
		errno = EINVAL;
		return EOF;
	}

	if (fdflags & O_NONBLOCK) {
		struct stat st;
		if (fstat(fd, &st) == -1)
			return -1;
		if (!S_ISREG(st.st_mode)) {
			errno = EFTYPE;
			return EOF;
		}
	}

	_FILEEXT_SETUP(&f, &fext);
	__sfpinit(&f);
	f._p = buf;
	f._w = sizeof(buf);
	f._flags = __SWR;
	f._file = fd;
	f._bf._base = buf;
	f._bf._size = sizeof(buf);
	f._cookie = &f;
	f._read = NULL;
	f._write = __swrite;
	f._seek = NULL;
	f._close = NULL;

	if ((ret = vfprintf(&f, fmt, ap)) < 0)
		return ret;

	return fflush(&f) ? EOF : ret;
}
