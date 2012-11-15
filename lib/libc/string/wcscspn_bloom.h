/*	$NetBSD: wcscspn_bloom.h,v 1.4 2011/11/25 17:48:22 joerg Exp $	*/

/*-
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
 */

/*
 * Bloom filter for fast test if a given character is part of the reject set.
 * The test may have false positives, but doesn't have false negatives.
 * The first hash function is designed to be very fast to evaluate.
 * It is collision free if the input is part of the same European language
 * and shouldn't be too bad even other input.  The second hash function
 * tries to provide a much better mixing, but involves the slower
 * multiplication.
 */

#include <limits.h>

#define	BLOOM_SIZE		64
#define	BLOOM_ARRAY_SIZE	(BLOOM_SIZE / sizeof(size_t))
#define	BLOOM_BITS		(BLOOM_SIZE * CHAR_BIT)
#define	BLOOM_DIV		(sizeof(size_t) * CHAR_BIT)

static inline size_t
wcscspn_bloom1(size_t x)
{
	return x % BLOOM_BITS;
}

static inline size_t
wcscspn_bloom2(size_t x)
{
	return (size_t)((uint32_t)(x * 2654435761U) /
	    (0x100000000ULL / BLOOM_BITS));
}

static inline void
wcsspn_bloom_init(size_t *bloom, const wchar_t *charset)
{
	size_t val;

	memset(bloom, 0, BLOOM_SIZE);
	do {
		val = wcscspn_bloom1((size_t)*charset);
		bloom[val / BLOOM_DIV] |= (size_t)1 << (val % BLOOM_DIV);
		val = wcscspn_bloom2((size_t)*charset);
		bloom[val / BLOOM_DIV] |= (size_t)1 << (val % BLOOM_DIV);
	}
	while (*++charset);
}

static inline int
wcsspn_in_bloom(const size_t *bloom, wchar_t ch)
{
	size_t val;

	val = wcscspn_bloom1((size_t)ch);
	if (bloom[val / BLOOM_DIV] & ((size_t)1 << (val % BLOOM_DIV)))
		return 1;
	val = wcscspn_bloom2((size_t)ch);
	if (bloom[val / BLOOM_DIV] & ((size_t)1 << (val % BLOOM_DIV)))
		return 1;
	return 0;
}
