/*	$NetBSD: strerror.c,v 1.16 2013/09/02 07:59:32 joerg Exp $	*/

/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
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
__RCSID("$NetBSD: strerror.c,v 1.16 2013/09/02 07:59:32 joerg Exp $");

#define __SETLOCALE_SOURCE__

#include "namespace.h"
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"
#ifdef _REENTRANT
#include "reentrant.h"
#endif
#include "setlocale_local.h"

__weak_alias(strerror_l, _strerror_l)

__aconst char *
strerror(int num)
{

	return strerror_l(num, _current_locale());
}

#ifdef _REENTRANT
static thread_key_t strerror_key;
static once_t strerror_once = ONCE_INITIALIZER;

static void
strerror_setup(void)
{

	thr_keycreate(&strerror_key, free);
}
#endif

__aconst char *
strerror_l(int num, locale_t loc)
{
	int error;
#ifdef _REENTRANT
	char *buf;

	thr_once(&strerror_once, strerror_setup);
	buf = thr_getspecific(strerror_key);
	if (buf == NULL) {
		buf = malloc(NL_TEXTMAX);
		if (buf == NULL) {
			static char fallback_buf[NL_TEXTMAX];
			buf = fallback_buf;
		}
		thr_setspecific(strerror_key, buf);
	}
#else
	static char buf[NL_TEXTMAX];
#endif

	error = _strerror_lr(num, buf, NL_TEXTMAX, loc);
	if (error)
		errno = error;
	return buf;
}
