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

#include <assert.h>
#include <wchar.h>
#include <wctype.h>

int
wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	int lc1  = 0;
	int lc2  = 0;
	int diff = 0;

	assert(s1);
	assert(s2);

	while (n--) {
		lc1 = towlower (*s1);
		lc2 = towlower (*s2);

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
