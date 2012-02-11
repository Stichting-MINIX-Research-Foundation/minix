/*	$NetBSD: getenv.c,v 1.35 2010/11/14 22:04:36 tron Exp $	*/

/*
 * Copyright (c) 1987, 1993
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
static char sccsid[] = "@(#)getenv.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getenv.c,v 1.35 2010/11/14 22:04:36 tron Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "reentrant.h"
#include "local.h"

__weak_alias(getenv_r, _getenv_r)

/*
 * getenv --
 *	Returns ptr to value associated with name, if any, else NULL.
 *	XXX: we cannot use getenv_r to implement this, because getenv()
 *	cannot use a shared buffer, because if it did, subsequent calls
 *	to getenv would trash previous results.
 */
char *
getenv(const char *name)
{
	size_t l_name;
	char *result;

	_DIAGASSERT(name != NULL);

	l_name = __envvarnamelen(name, false);
	if (l_name == 0)
		return NULL;

	result = NULL;
	if (__readlockenv()) {
		result = __findenvvar(name, l_name);
		(void)__unlockenv();
	}
	
	return result;
}

int
getenv_r(const char *name, char *buf, size_t len)
{
	size_t l_name;
	int rv;

	_DIAGASSERT(name != NULL);

	l_name = __envvarnamelen(name, false);
	if (l_name == 0) {
		errno = ENOENT;
		return -1;
	}

	rv = -1;
	if (__readlockenv()) {
		const char *value;

		value = __findenvvar(name, l_name);
		if (value != NULL) {
			if (strlcpy(buf, value, len) < len) {
				rv = 0;
			} else {
				errno = ERANGE;
			}
		} else {
			errno = ENOENT;
		}
		(void)__unlockenv();
	}
	
	return rv;
}
