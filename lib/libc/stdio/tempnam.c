/*	$NetBSD: tempnam.c,v 1.22 2012/03/15 18:22:30 christos Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)tempnam.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: tempnam.c,v 1.22 2012/03/15 18:22:30 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include "reentrant.h"
#include "local.h"

__warn_references(tempnam,
    "warning: tempnam() possibly used unsafely, use mkstemp() or mkdtemp()")

static const char *
trailsl(const char *f)
{
	const char *s = f;
	while (*s)
		s++;
	return (f != s && s[-1] == '/') ? "" : "/";
}

static char *
gentemp(char *name, size_t len, const char *tmp, const char  *pfx)
{
	(void)snprintf(name, len, "%s%s%sXXXXXXXXXX", tmp, trailsl(tmp), pfx);
	return _mktemp(name);
}

char *
tempnam(const char *dir, const char *pfx)
{
	int sverrno;
	char *name, *f;
	const char *tmp;

	if (!(name = malloc((size_t)MAXPATHLEN)))
		return NULL;

	if (!pfx)
		pfx = "tmp.";

	if ((tmp = getenv("TMPDIR")) != NULL &&
	    (f = gentemp(name, (size_t)MAXPATHLEN, tmp, pfx)) != NULL)
		return f;

	if (dir != NULL &&
	    (f = gentemp(name, (size_t)MAXPATHLEN, dir, pfx)) != NULL)
		return f;

	if ((f = gentemp(name, (size_t)MAXPATHLEN, P_tmpdir, pfx)) != NULL)
		return f;

	if ((f = gentemp(name, (size_t)MAXPATHLEN, _PATH_TMP, pfx)) != NULL)
		return f;

	sverrno = errno;
	free(name);
	errno = sverrno;
	return NULL;
}
