/*	$NetBSD: setenv.c,v 1.43 2010/11/14 18:11:43 tron Exp $	*/

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
static char sccsid[] = "@(#)setenv.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: setenv.c,v 1.43 2010/11/14 18:11:43 tron Exp $");
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

#ifdef __weak_alias
__weak_alias(setenv,_setenv)
#endif

/*
 * setenv --
 *	Set the value of the environmental variable "name" to be
 *	"value".  If rewrite is set, replace any current value.
 */
int
setenv(const char *name, const char *value, int rewrite)
{
	size_t l_name, l_value, length;
	ssize_t offset;
	char *envvar;

	_DIAGASSERT(name != NULL);
	_DIAGASSERT(value != NULL);

	l_name = __envvarnamelen(name, false);
	if (l_name == 0 || value == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (!__writelockenv())
		return -1;

	/* Find slot in the enviroment. */
	offset = __getenvslot(name, l_name, true);
	if (offset == -1)
		goto bad;

	l_value = strlen(value);
	length = l_name + l_value + 2;

	/* Handle overwriting a current environt variable. */
	envvar = environ[offset];
	if (envvar != NULL) {
		if (!rewrite)
			goto good;
		/*
		 * Check whether the buffer was allocated via setenv(3) and
		 * whether there is enough space. If so simply overwrite the
		 * existing value.
		 */
		if (__canoverwriteenvvar(envvar, length)) {
			envvar += l_name + 1;
			goto copy;
		}
	}

	/* Allocate memory for name + `=' + value + NUL. */
	if ((envvar = __allocenvvar(length)) == NULL)
		goto bad;

	if (environ[offset] != NULL)
		__freeenvvar(environ[offset]);

	environ[offset] = envvar;

	(void)memcpy(envvar, name, l_name);

	envvar += l_name;
	*envvar++ = '=';

copy:
	(void)memcpy(envvar, value, l_value + 1);

good:
	(void)__unlockenv();
	return 0;

bad:
	(void)__unlockenv();
	return -1;
}
