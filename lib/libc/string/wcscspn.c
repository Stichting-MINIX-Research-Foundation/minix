/*	$NetBSD: wcscspn.c,v 1.4 2011/11/24 18:44:25 joerg Exp $	*/

/*-
 * Copyright (c) 1999 Citrus Project,
 * Copyright (c) 2011 Joerg Sonnenberger,
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
 *	citrus Id: wcscspn.c,v 1.1 1999/12/29 21:47:45 tshiozak Exp
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: wcscspn.c,v 1.4 2011/11/24 18:44:25 joerg Exp $");

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <wchar.h>

#include "wcscspn_bloom.h"

size_t
wcscspn(const wchar_t *s, const wchar_t *set)
{
	size_t bloom[BLOOM_ARRAY_SIZE];
	const wchar_t *p;
	const wchar_t *q;

	_DIAGASSERT(s != NULL);
	_DIAGASSERT(set != NULL);

	if (set[0] == '\0')
		return wcslen(s);
	if (set[1] == '\0') {
		for (p = s; *p; ++p)
			if (*p == set[0])
				break;
		return p - s;
	}

	wcsspn_bloom_init(bloom, set);

	for (p = s; *p; ++p) {
		if (!wcsspn_in_bloom(bloom, *p))
			continue;

		q = set;
		do {
			if (*p == *q)
				goto done;
		} while (*++q);
	}

done:
	return (p - s);
}
