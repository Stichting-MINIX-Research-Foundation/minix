/*	$NetBSD: getcwd.c,v 1.47 2011/01/20 02:57:00 christos Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
static char sccsid[] = "@(#)getcwd.c	8.5 (Berkeley) 2/7/95";
#else
__RCSID("$NetBSD: getcwd.c,v 1.47 2011/01/20 02:57:00 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#ifdef __weak_alias
__weak_alias(getcwd,_sys_getcwd)
__weak_alias(_getcwd,_sys_getcwd)
__weak_alias(realpath,_realpath)

#if defined(_FORTIFY_SOURCE) && !defined(__lint__)
#undef getcwd
#define getcwd _sys_getcwd
#endif

#endif

/*
 * char *realpath(const char *path, char resolved[MAXPATHLEN]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
char *
realpath(const char *path, char *resolved)
{
	struct stat sb;
	int idx = 0, n, nlnk = 0;
	const char *q;
	char *p, wbuf[2][MAXPATHLEN];
	size_t len;

	_DIAGASSERT(resolved != NULL);

	/* POSIX sez we must test for this */
	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}

	/*
	 * Build real path one by one with paying an attention to .,
	 * .. and symbolic link.
	 */

	/*
	 * `p' is where we'll put a new component with prepending
	 * a delimiter.
	 */
	p = resolved;

	if (*path == 0) {
		*p = 0;
		errno = ENOENT;
		return (NULL);
	}

	/* If relative path, start from current working directory. */
	if (*path != '/') {
		/* check for resolved pointer to appease coverity */
		if (resolved && getcwd(resolved, MAXPATHLEN) == NULL) {
			p[0] = '.';
			p[1] = 0;
			return (NULL);
		}
		len = strlen(resolved);
		if (len > 1)
			p += len;
	}

loop:
	/* Skip any slash. */
	while (*path == '/')
		path++;

	if (*path == 0) {
		if (p == resolved)
			*p++ = '/';
		*p = 0;
		return (resolved);
	}

	/* Find the end of this component. */
	q = path;
	do
		q++;
	while (*q != '/' && *q != 0);

	/* Test . or .. */
	if (path[0] == '.') {
		if (q - path == 1) {
			path = q;
			goto loop;
		}
		if (path[1] == '.' && q - path == 2) {
			/* Trim the last component. */
			if (p != resolved)
				while (*--p != '/')
					;
			path = q;
			goto loop;
		}
	}

	/* Append this component. */
	if (p - resolved + 1 + q - path + 1 > MAXPATHLEN) {
		errno = ENAMETOOLONG;
		if (p == resolved)
			*p++ = '/';
		*p = 0;
		return (NULL);
	}
	p[0] = '/';
	memcpy(&p[1], path,
	    /* LINTED We know q > path. */
	    q - path);
	p[1 + q - path] = 0;

	/*
	 * If this component is a symlink, toss it and prepend link
	 * target to unresolved path.
	 */
	if (lstat(resolved, &sb) == -1) {
		return (NULL);
	}
	if (S_ISLNK(sb.st_mode)) {
		if (nlnk++ >= MAXSYMLINKS) {
			errno = ELOOP;
			return (NULL);
		}
		n = readlink(resolved, wbuf[idx], sizeof(wbuf[0]) - 1);
		if (n < 0)
			return (NULL);
		if (n == 0) {
			errno = ENOENT;
			return (NULL);
		}

		/* Append unresolved path to link target and switch to it. */
		if (n + (len = strlen(q)) + 1 > sizeof(wbuf[0])) {
			errno = ENAMETOOLONG;
			return (NULL);
		}
		memcpy(&wbuf[idx][n], q, len + 1);
		path = wbuf[idx];
		idx ^= 1;

		/* If absolute symlink, start from root. */
		if (*path == '/')
			p = resolved;
		goto loop;
	}
	if (*q == '/' && !S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return (NULL);
	}

	/* Advance both resolved and unresolved path. */
	p += 1 + q - path;
	path = q;
	goto loop;
}

char *
getcwd(char *pt, size_t size)
{
	char *npt;

	/*
	 * If a buffer is specified, the size has to be non-zero.
	 */
	if (pt != NULL) {
		if (size == 0) {
			/* __getcwd(pt, 0) results ERANGE. */
			errno = EINVAL;
			return (NULL);
		}
		if (__getcwd(pt, size) >= 0)
			return (pt);
		return (NULL);
	}

	/*
	 * If no buffer specified by the user, allocate one as necessary.
	 */
	size = 1024 >> 1;
	do {
		if ((npt = realloc(pt, size <<= 1)) == NULL)
			break;
		pt = npt;
		if (__getcwd(pt, size) >= 0)
			return (pt);
	} while (size <= MAXPATHLEN * 4 && errno == ERANGE);

	free(pt);
	return (NULL);
}
