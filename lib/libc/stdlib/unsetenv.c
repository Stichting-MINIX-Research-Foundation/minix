/*	$NetBSD: unsetenv.c,v 1.10 2010/11/14 18:11:43 tron Exp $	*/

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
static char sccsid[] = "from: @(#)setenv.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: unsetenv.c,v 1.10 2010/11/14 18:11:43 tron Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <bitstring.h>

#include "env.h"
#include "reentrant.h"
#include "local.h"

/*
 * unsetenv(name) --
 *	Delete environmental variable "name".
 */
int
unsetenv(const char *name)
{
	size_t l_name;
	ssize_t r_offset, w_offset;

	_DIAGASSERT(name != NULL);

	l_name = __envvarnamelen(name, false);
	if (l_name == 0) {
		errno = EINVAL;
		return -1;
	}

	if (!__writelockenv())
		return -1;

	/* Search for the given name in the environment. */
	r_offset = __getenvslot(name, l_name, false);
	if (r_offset == -1) {
		/* Not found. */
		(void)__unlockenv();
		return 0;
	}
	__freeenvvar(environ[r_offset]);

	/*
	 * Remove all matches from the environment and free the associated
	 * memory if possible.
	 */
	w_offset = r_offset;
	while (environ[++r_offset] != NULL) {
		if (strncmp(environ[r_offset], name, l_name) != 0 ||
		    environ[r_offset][l_name] != '=') {
			/* Not a match, keep this entry. */
			environ[w_offset++] = environ[r_offset];
		} else {
			/* We found another match. */
			__freeenvvar(environ[r_offset]);
		}
	}

	/* Clear out remaining stale entries in the environment vector. */
	do {
		environ[w_offset++] = NULL;
	} while (w_offset < r_offset);

	(void)__unlockenv();
	return 0;
}
