/*	$NetBSD: confstr.c,v 1.13 2012/06/25 22:32:43 abs Exp $	*/

/*-
 * Copyright (c) 1993
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
static char sccsid[] = "@(#)confstr.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: confstr.c,v 1.13 2012/06/25 22:32:43 abs Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(confstr,_confstr)
#endif

size_t
confstr(int name, char *buf, size_t len)
{
	size_t tlen;
	int mib[2], sverrno;
	char *p;

	/*
	 * POSIX 1003.2 requires errors to return 0 --
	 * that is *really* useful.
	 */
	switch (name) {
	case _CS_PATH:
		mib[0] = CTL_USER;
		mib[1] = USER_CS_PATH;
		if (sysctl(mib, 2, NULL, &tlen, NULL, 0) == -1)
			return 0;
		if (len != 0 && buf != NULL) {
			if ((p = malloc(tlen)) == NULL)
				return 0;
			if (sysctl(mib, 2, p, &tlen, NULL, 0) == -1) {
				sverrno = errno;
				free(p);
				errno = sverrno;
				return 0;
			}
			/*
			 * POSIX 1003.2 requires partial return of
			 * the string -- that is even more useful.
			 */
			(void)strncpy(buf, p, len - 1);
			buf[len - 1] = '\0';
			free(p);
		}
		return tlen + 1;
	default:
		errno = EINVAL;
		return 0;
	}
	/* NOTREACHED */
}
