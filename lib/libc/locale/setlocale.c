/* $NetBSD: setlocale.c,v 1.60 2012/03/04 21:14:56 tnozaki Exp $ */

/*-
 * Copyright (c)2008 Citrus Project,
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: setlocale.c,v 1.60 2012/03/04 21:14:56 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <locale.h>
#include <limits.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "setlocale_local.h"

const char *_PathLocale = NULL;

static _locale_set_t all_categories[_LC_LAST] = {
	[LC_ALL     ] = &_generic_LC_ALL_setlocale,
	[LC_COLLATE ] = &_dummy_LC_COLLATE_setlocale,
	[LC_CTYPE   ] = &_citrus_LC_CTYPE_setlocale,
	[LC_MONETARY] = &_citrus_LC_MONETARY_setlocale,
	[LC_NUMERIC ] = &_citrus_LC_NUMERIC_setlocale,
	[LC_TIME    ] = &_citrus_LC_TIME_setlocale,
	[LC_MESSAGES] = &_citrus_LC_MESSAGES_setlocale,
};

_locale_set_t
_find_category(int category)
{
	if (category >= LC_ALL && category < _LC_LAST)
		return all_categories[category];
	return NULL;
}

const char *
_get_locale_env(const char *category)
{
	const char *name;

	/* 1. check LC_ALL */
	name = (const char *)getenv("LC_ALL");
	if (name == NULL || *name == '\0') {
		/* 2. check LC_* */
		name = (const char *)getenv(category);
		if (name == NULL || *name == '\0') {
			/* 3. check LANG */
			name = getenv("LANG");
		}
	}
	if (name == NULL || *name == '\0' || strchr(name, '/'))
		/* 4. if none is set, fall to "C" */
		name = _C_LOCALE;
	return name;
}

char *
__setlocale(int category, const char *name)
{
	_locale_set_t sl;
	struct _locale_impl_t *impl;

	sl = _find_category(category);
	if (sl == NULL)
		return NULL;
	if (issetugid() || ((_PathLocale == NULL &&
	    (_PathLocale = getenv("PATH_LOCALE")) == NULL) ||
	    *_PathLocale == '\0'))
		_PathLocale = _PATH_LOCALE;
	impl = *_current_locale();
	return __UNCONST((*sl)(name, impl));
}

char *
setlocale(int category, const char *locale)
{

	/* locale may be NULL */

	__mb_len_max_runtime = MB_LEN_MAX;
	return __setlocale(category, locale);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(setlocale, __setlocale50)
#endif
