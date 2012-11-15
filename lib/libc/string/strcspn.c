/*	$NetBSD: strcspn.c,v 1.18 2012/03/21 00:35:50 christos Exp $	*/

/*-
 * Copyright (c) 2008 Joerg Sonnenberger
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: strcspn.c,v 1.18 2012/03/21 00:35:50 christos Exp $");

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

/* 64bit version is in strspn.c */
#if ULONG_MAX != 0xffffffffffffffffull

size_t
strcspn(const char *s, const char *charset)
{
	static const uint8_t idx[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
	const char *t;
	uint8_t set[32];
#define UC(a) ((unsigned int)(unsigned char)(a))

	_DIAGASSERT(s != NULL);
	_DIAGASSERT(charset != NULL);

	if (charset[0] == '\0')
		return strlen(s);
	if (charset[1] == '\0') {
		for (t = s; *t != '\0'; ++t)
			if (*t == *charset)
				break;
		return t - s;
	}

	(void)memset(set, 0, sizeof(set));

	for (; *charset != '\0'; ++charset)
		set[UC(*charset) >> 3] |= idx[UC(*charset) & 7];

	for (t = s; *t != '\0'; ++t)
		if (set[UC(*t) >> 3] & idx[UC(*t) & 7])
			break;
	return t - s;
}

#endif
