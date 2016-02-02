/*	$NetBSD: compat_putenv.c,v 1.3 2015/01/20 18:31:24 christos Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__RCSID("$NetBSD: compat_putenv.c,v 1.3 2015/01/20 18:31:24 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__
#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <compat/include/stdlib.h>

#include "env.h"
#include "local.h"

#ifdef __weak_alias
__weak_alias(putenv,_putenv)
#endif

__warn_references(putenv,
    "warning: reference to compatibility putenv();"
    " include <stdlib.h> for correct reference")

/*
 * putenv(name) --
 *	This version implicitly copies the string for compatibility.
 */
int
putenv(char *name)
{
	size_t l_name;
	char *copy;
	int rv;

	_DIAGASSERT(name != NULL);

        l_name = __envvarnamelen(name, true); 
        if (l_name == 0) { 
                errno = EINVAL;
                return -1;
        }

	if ((copy = strdup(name)) == NULL)
		return -1;
	copy[l_name++] = '\0';

	rv = setenv(copy, copy + l_name, 1);

	free(copy);
	return rv;
}
