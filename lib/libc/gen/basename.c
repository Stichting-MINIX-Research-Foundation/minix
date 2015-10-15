/*	$NetBSD: basename.c,v 1.11 2014/07/16 10:52:26 christos Exp $	*/

/*-
 * Copyright (c) 1997, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein and Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: basename.c,v 1.11 2014/07/16 10:52:26 christos Exp $");
#endif /* !LIBC_SCCS && !lint */

#include "namespace.h"
#include <sys/param.h>
#include <libgen.h>
#include <string.h>
#include <limits.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(basename,_basename)
#endif

static size_t
xbasename_r(const char *path, char *buf, size_t buflen)
{
	const char *startp, *endp;
	size_t len;

	/*
	 * If `path' is a null pointer or points to an empty string,
	 * return a pointer to the string ".".
	 */
	if (path == NULL || *path == '\0') {
		startp = ".";
		len = 1;
		goto out;
	}

	/* Strip trailing slashes, if any. */
	endp = path + strlen(path) - 1;
	while (endp != path && *endp == '/')
		endp--;

	/* Only slashes -> "/" */
	if (endp == path && *endp == '/') {
		startp = "/";
		len = 1;
		goto out;
	}

	/* Now find the beginning of this (final) component. */
	for (startp = endp; startp > path && *(startp - 1) != '/'; startp--)
		continue;

	/* ...and copy the result into the result buffer. */
	len = (endp - startp) + 1 /* last char */;
out:
	if (buf != NULL && buflen != 0) {
		buflen = MIN(len, buflen - 1);
		memcpy(buf, startp, buflen);
		buf[buflen] = '\0';
	}
	return len;
}

#if !HAVE_BASENAME

char *
basename(char *path) {
	static char result[PATH_MAX];

	(void)xbasename_r(path, result, sizeof(result));
	return result;
}

#endif
