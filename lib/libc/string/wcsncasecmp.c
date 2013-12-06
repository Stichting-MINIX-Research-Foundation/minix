/*	$NetBSD: wcsncasecmp.c,v 1.4 2013/05/17 12:55:57 joerg Exp $	*/

/*
 * Copyright (C) 2006 Aleksey Cheusov
 *
 * This material is provided "as is", with absolutely no warranty expressed
 * or implied. Any use is at your own risk.
 *
 * Permission to use or copy this software for any purpose is hereby granted 
 * without fee. Permission to modify the code and to distribute modified
 * code is also granted without any restrictions.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint) 
__RCSID("$NetBSD: wcsncasecmp.c,v 1.4 2013/05/17 12:55:57 joerg Exp $"); 
#endif /* LIBC_SCCS and not lint */ 

#include "namespace.h"
#include <assert.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include "setlocale_local.h"

__weak_alias(wcsncasecmp,_wcsncasecmp)
__weak_alias(wcsncasecmp_l,_wcsncasecmp_l)

int
wcsncasecmp_l(const wchar_t *s1, const wchar_t *s2, size_t n, locale_t loc)
{
	int lc1  = 0;
	int lc2  = 0;
	int diff = 0;

	_DIAGASSERT(s1);
	_DIAGASSERT(s2);

	while (n--) {
		lc1 = towlower_l(*s1, loc);
		lc2 = towlower_l(*s2, loc);

		diff = lc1 - lc2;
		if (diff)
			return diff;

		if (!lc1)
			return 0;

		++s1;
		++s2;
	}

	return 0;
}

int
wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	return wcsncasecmp_l(s1, s2, n, _current_locale());
}
