/* $NetBSD: memcpy.c,v 1.1 2014/09/03 19:34:25 matt Exp $ */
/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__RCSID("$NetBSD: memcpy.c,v 1.1 2014/09/03 19:34:25 matt Exp $");

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline unsigned long
combine_words(unsigned long w1, unsigned long w2, int shift1, int shift2)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return (w1 << shift1) | (w2 >> shift2);
#else
	return (w1 >> shift1) | (w2 << shift2);
#endif
}

void *
memcpy(void * restrict a, const void * restrict b, size_t len)
{
	const unsigned char *cb = b;
	unsigned char *ca = a;

	if (len == 0)
		return a;

	/*
	 * Make sure the destination is long aligned.
	 */
	while ((uintptr_t)ca & (sizeof(long) - 1)) {
		*ca++ = *cb++;
		if (--len == 0)
			return a;
	}

	unsigned long *la = (long *)ca;
	const int offset = (uintptr_t)cb & (sizeof(*la) - 1);
	const unsigned long *lb = (const unsigned long *) (cb - offset);
	unsigned long * const ea = la + len / sizeof(*la);

	if (offset == 0) {
		/*
		 * a & b are now both long alignment.
		 * First try to copy 4 longs at a time,
		 */
		for (; la + 4 <= ea; la += 4, lb += 4) {
			la[0] = lb[0];
			la[1] = lb[1];
			la[2] = lb[2];
			la[3] = lb[3];
		}
		/*
		 * Now try to copy one long at a time.
		 */
		while (la <= ea) {
			*la++ = *lb++;
		}
	} else {
		const int shift1 = offset * 8;
		const int shift2 = sizeof(*la) * 8 - shift1;
		unsigned long w1 = *lb++;

		/*
		 * We try to write 4 words per loop.
		 */
		for (; la + 4 <= ea; la += 4, lb += 4) {
			unsigned long w2 = lb[0];

			la[0] = combine_words(w1, w2, shift1, shift2);

			w1 = lb[1];

			la[1] = combine_words(w2, w1, shift1, shift2);

			w2 = lb[2];

			la[2] = combine_words(w1, w2, shift1, shift2);

			w1 = lb[3];

			la[3] = combine_words(w2, w1, shift1, shift2);
		}

		/*
		 * Now try to copy one long at a time.
		 */
		while (la <= ea) {
			unsigned long w2 = *lb++;

			*la++ = combine_words(w1, w2, shift1, shift2);

			w1 = w2;
		}
	}
	len &= sizeof(*la) - 1;
	if (len) {
		cb = (const unsigned char *)lb + offset;
		ca = (unsigned char *)la;
		while (len-- > 0) {
			*ca++ = *cb++;
		}
	}
	return a;
}
