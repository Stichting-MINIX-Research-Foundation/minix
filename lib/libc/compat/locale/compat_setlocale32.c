/* $NetBSD: compat_setlocale32.c,v 1.2 2013/03/06 11:29:01 yamt Exp $ */

/*-
 * Copyright (c)1999 Citrus Project,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NetBSD: setlocale32.c,v 1.6 2010/05/22 13:50:02 tnozaki Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_setlocale32.c,v 1.2 2013/03/06 11:29:01 yamt Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <compat/include/locale.h>

#include "setlocale_local.h"

__warn_references(__setlocale_mb_len_max_32,
    "warning: reference to compatibility __setlocale_mb_len_max_32();"
    "include <locale.h> for correct reference")

/*
 * MB_LEN_MAX used to be a MD macro.  it was 32 for most ports but 6 for hppa.
 * hppa uses arch/hppa/locale/compat_setlocale32.c instead of this file.
 */
#if defined(__hppa__)
#error using wrong variant of compat_setlocale32.c
#endif /* defined(__hppa__) */

char *
__setlocale_mb_len_max_32(int category, const char *locale)
{

	/* locale may be NULL */

	__mb_len_max_runtime = 32;
	return __setlocale(category, locale);
}
