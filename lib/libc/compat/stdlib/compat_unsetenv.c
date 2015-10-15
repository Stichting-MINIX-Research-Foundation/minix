/*	$NetBSD: compat_unsetenv.c,v 1.4 2015/01/20 18:31:24 christos Exp $	*/

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
__RCSID("$NetBSD: compat_unsetenv.c,v 1.4 2015/01/20 18:31:24 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__
#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <compat/include/stdlib.h>
#include <string.h>
#include <bitstring.h>
#include "local.h"

#ifdef __weak_alias
__weak_alias(unsetenv,_unsetenv)
#endif

#include "env.h"

__warn_references(unsetenv,
    "warning: reference to compatibility unsetenv();"
    " include <stdlib.h> for correct reference")

/*
 * unsetenv(name) --
 *	Delete environmental variable "name".
 */
void
unsetenv(const char *name)
{
	size_t l_name;

	_DIAGASSERT(name != NULL);

	l_name = strlen(name);
	if (__writelockenv()) {
		ssize_t offset;

		while ((offset = __getenvslot(name, l_name, false)) != -1) {
			char **p;		
			for (p = &environ[offset]; ; ++p) {
				if ((*p = *(p + 1)) == NULL)
					break;
			}
		}
		(void)__unlockenv();
	}
}
