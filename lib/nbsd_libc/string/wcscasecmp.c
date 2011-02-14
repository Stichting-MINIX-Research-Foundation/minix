/*	$NetBSD: wcscasecmp.c,v 1.2 2006/08/26 22:45:52 christos Exp $	*/

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
__RCSID("$NetBSD: wcscasecmp.c,v 1.2 2006/08/26 22:45:52 christos Exp $"); 
#endif /* LIBC_SCCS and not lint */ 

#include "namespace.h"
#include <assert.h>
#include <wchar.h>
#include <wctype.h>

__weak_alias(wcscasecmp,_wcscasecmp)

int
wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{
	int lc1  = 0;
	int lc2  = 0;
	int diff = 0;

	_DIAGASSERT(s1);
	_DIAGASSERT(s2);

	for (;;) {
		lc1 = towlower(*s1);
		lc2 = towlower(*s2);

		diff = lc1 - lc2;
		if (diff)
			return diff;

		if (!lc1)
			return 0;

		++s1;
		++s2;
	}
}
