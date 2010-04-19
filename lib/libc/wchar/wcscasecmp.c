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
wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{

	assert(s1 != NULL);
	assert(s2 != NULL);

	for (;;) {
		int lc1 = towlower(*s1);
		int lc2 = towlower(*s2);

		int diff = lc1 - lc2;
		if (diff != 0)
			return diff;

		if (!lc1)
			return 0;

		++s1;
		++s2;
	}
}
