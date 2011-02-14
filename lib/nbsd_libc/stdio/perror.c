/*	$NetBSD: perror.c,v 1.24 2006/01/26 11:13:42 kleink Exp $	*/

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
static char sccsid[] = "@(#)perror.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: perror.c,v 1.24 2006/01/26 11:13:42 kleink Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "extern.h"

/*
 * Since perror() is not allowed to change the contents of strerror()'s
 * static buffer, both functions supply their own buffers to strerror_r().
 */

void
perror(const char *s)
{
	const char *separator;
	char buf[NL_TEXTMAX];

	if (s == NULL)
		s = "";
	if (*s == '\0')
		separator = "";
	else
		separator = ": ";

	(void)strerror_r(errno, buf, sizeof(buf));
	(void)fprintf(stderr, "%s%s%s\n", s, separator, buf);
}
