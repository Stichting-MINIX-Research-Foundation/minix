/*	$NetBSD: strpbrk.c,v 1.20 2011/11/22 00:37:09 joerg Exp $	*/

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
__RCSID("$NetBSD: strpbrk.c,v 1.20 2011/11/22 00:37:09 joerg Exp $");

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#define FAST_STRPBRK 1
#define UC(a) ((unsigned int)(unsigned char)(a))

#ifdef FAST_STRPBRK
#define ADD_NEW_TO_SET(i) (set[inv[i] = idx++] = (i))
#define IS_IN_SET(i) (inv[i] < idx && set[inv[i]] == (i))
#define ADD_TO_SET(i) (void)(IS_IN_SET(i) || /*LINTED no effect*/ADD_NEW_TO_SET(i))
#else
#define IS_IN_SET(i) (set[(i) >> 3] & idx[(i) & 7])
#define ADD_TO_SET(i) (void)(set[(i) >> 3] |= idx[(i) & 7])
#endif

char *
strpbrk(const char *s, const char *charset)
{
#ifdef FAST_STRPBRK
	uint8_t set[256], inv[256], idx = 0;
#else
	static const size_t idx[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
	uint8_t set[32];

	(void)memset(set, 0, sizeof(set));
#endif

	_DIAGASSERT(s != NULL);
	_DIAGASSERT(charset != NULL);

	if (charset[0] == '\0')
		return NULL;
	if (charset[1] == '\0')
		return strchr(s, charset[0]);

	for (; *charset != '\0'; ++charset)
		ADD_TO_SET(UC(*charset));

	for (; *s != '\0'; ++s)
		if (IS_IN_SET(UC(*s)))
			return __UNCONST(s);
	return NULL;
}
