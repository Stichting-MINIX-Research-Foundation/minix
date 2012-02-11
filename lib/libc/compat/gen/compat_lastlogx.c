/*	$NetBSD: compat_lastlogx.c,v 1.2 2009/01/11 02:46:25 christos Exp $	 */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: compat_lastlogx.c,v 1.2 2009/01/11 02:46:25 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
/* don't define earlier, has side effects in fcntl.h */
#define __LIBC12_SOURCE__
#include <utmpx.h>
#include <compat/include/utmpx.h>

__warn_references(getlastlogx,
    "warning: reference to compatibility getlastlogx(); include <utmpx.h> for correct reference")
__warn_references(lastlogxname,
    "warning: reference to deprecated lastlogxname()")

static char llfile[MAXPATHLEN] = _PATH_LASTLOGX;

int
lastlogxname(const char *fname)
{
	size_t len;

	_DIAGASSERT(fname != NULL);

	len = strlen(fname);

	if (len >= sizeof(llfile))
		return 0;

	/* must end in x! */
	if (fname[len - 1] != 'x')
		return 0;

	(void)strlcpy(llfile, fname, sizeof(llfile));
	return 1;
}

struct lastlogx *
getlastlogx(uid_t uid, struct lastlogx *ll)
{

	return __getlastlogx13(llfile, uid, ll);
}
