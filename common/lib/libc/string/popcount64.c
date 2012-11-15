/*	$NetBSD: popcount64.c,v 1.7 2012/03/09 15:41:16 christos Exp $	*/
/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: popcount64.c,v 1.7 2012/03/09 15:41:16 christos Exp $");

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <limits.h>
#include <stdint.h>
#include <strings.h>
#else
#include <lib/libkern/libkern.h>
#include <machine/limits.h>
#endif

/*
 * If uint64_t is larger than size_t, the follow assumes that
 * splitting into 32bit halfes is faster.
 *
 * The native pocount64 version is based on the same ideas as popcount32(3),
 * see popcount32.c for comments.
 */

#if SIZE_MAX < 0xffffffffffffffffULL
unsigned int
popcount64(uint64_t v)
{
	return popcount32((uint32_t)(v >> 32)) +
	    popcount32((uint32_t)(v & 0xffffffffULL));
}
#else
unsigned int
popcount64(uint64_t v)
{
	unsigned int c;

	v = v - ((v >> 1) & (uint64_t)0x5555555555555555ULL);
	v = (v & (uint64_t)0x3333333333333333ULL) +
	    ((v >> 2) & (uint64_t)0x3333333333333333ULL);
	v = ((v + (v >> 4)) & (uint64_t)0x0f0f0f0f0f0f0f0fULL) *
	    (uint64_t)0x0101010101010101ULL;
	c = (unsigned int)(v >> 56);

	return c;
}
#endif

#if ULONG_MAX == 0xffffffffffffffffULL
__strong_alias(popcountl, popcount64)
#endif

#if ULLONG_MAX == 0xffffffffffffffffULL
__strong_alias(popcountll, popcount64)
#endif

