/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>.
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: memset2.c,v 1.5 2012/03/02 16:22:27 apb Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>
#else
#include <lib/libkern/libkern.h>
#include <machine/limits.h>
#endif 

#include <sys/endian.h>
#include <machine/types.h>

#ifdef TEST
#include <assert.h>
#define _DIAGASSERT(a)		assert(a)
#endif

#ifdef _FORTIFY_SOURCE
#undef bzero
#endif
#undef memset

/*
 * Assume uregister_t is the widest non-synthetic unsigned type.
 */
typedef uregister_t memword_t;

__CTASSERT((~(memword_t)0U >> 1) != ~(memword_t)0U);

#ifdef BZERO
static inline
#define	memset memset0
#endif

#ifdef TEST
static
#define memset test_memset
#endif

void *
memset(void *addr, int c, size_t len)
{
	memword_t *dstp = addr;
	memword_t *edstp;
	memword_t fill;
#ifndef __OPTIMIZE_SIZE__
	memword_t keep_mask = 0;
#endif
	size_t fill_count;

	_DIAGASSERT(addr != 0);

	if (__predict_false(len == 0))
		return addr;

	/*
	 * Pad out the fill byte (v) across a memword_t.
	 * The conditional at the end prevents GCC from complaing about
	 * shift count >= width of type 
	 */
	fill = c;
	fill |= fill << 8;
	fill |= fill << 16;
	fill |= fill << (sizeof(c) < sizeof(fill) ? 32 : 0);

	/*
	 * Get the number of unaligned bytes to fill in the first word.
	 */
	fill_count = -(uintptr_t)addr & (sizeof(memword_t) - 1);

	if (__predict_false(fill_count != 0)) {
#ifndef __OPTIMIZE_SIZE__
		/*
		 * We want to clear <fill_count> trailing bytes in the word.
		 * On big/little endian, these are the least/most significant,
		 * bits respectively.  So as we shift, the keep_mask will only
		 * have bits set for the bytes we won't be filling.
		 */
#if BYTE_ORDER == BIG_ENDIAN
		keep_mask = ~(memword_t)0U << (fill_count * 8);
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
		keep_mask = ~(memword_t)0U >> (fill_count * 8);
#endif
		/*
		 * Make sure dstp is aligned to a memword_t boundary.
		 */
		dstp = (memword_t *)((uintptr_t)addr & -sizeof(memword_t));
		if (len >= fill_count) {
			/*
			 * If we can fill the rest of this word, then we mask
			 * off the bytes we are filling and then fill in those
			 * bytes with the new fill value.
			 */
			*dstp = (*dstp & keep_mask) | (fill & ~keep_mask);
			len -= fill_count;
			if (__predict_false(len == 0))
				return addr;
			/*
			 * Since we were able to fill the rest of this word,
			 * we will advance to the next word and thus have no
			 * bytes to preserve.
			 *
			 * If we don't have enough to fill the rest of this
			 * word, we will fall through the following loop
			 * (since there are no full words to fill).  Then we
			 * use the keep_mask above to preserve the leading
			 * bytes of word.
			 */
			dstp++;
			keep_mask = 0;
		} else {
			len += (uintptr_t)addr & (sizeof(memword_t) - 1);
		}
#else /* __OPTIMIZE_SIZE__ */
		uint8_t *dp, *ep;
		if (len < fill_count)
			fill_count = len;
		for (dp = (uint8_t *)dstp, ep = dp + fill_count;
		     dp != ep; dp++)
			*dp = fill;
		if ((len -= fill_count) == 0)
			return addr;
		dstp = (memword_t *)ep;
#endif /* __OPTIMIZE_SIZE__ */
	}

	/*
	 * Simply fill memory one word at time (for as many full words we have
	 * to write).
	 */
	for (edstp = dstp + len / sizeof(memword_t); dstp != edstp; dstp++)
		*dstp = fill;

	/*
	 * We didn't subtract out the full words we just filled since we know
	 * by the time we get here we will have less than a words worth to
	 * write.  So we can concern ourselves with only the subword len bits.
	 */
	len &= sizeof(memword_t)-1;
	if (len > 0) {
#ifndef __OPTIMIZE_SIZE__
		/*
		 * We want to clear <len> leading bytes in the word.
		 * On big/little endian, these are the most/least significant
		 * bits, respectively,  But as we want the mask of the bytes to
		 * keep, we have to complement the mask.  So after we shift,
		 * the keep_mask will only have bits set for the bytes we won't
		 * be filling.
		 *
		 * But the keep_mask could already have bytes to preserve
		 * if the amount to fill was less than the amount of traiing
		 * space in the first word.
		 */
#if BYTE_ORDER == BIG_ENDIAN
		keep_mask |= ~(memword_t)0U >> (len * 8);
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
		keep_mask |= ~(memword_t)0U << (len * 8);
#endif
		/*
		 * Now we mask off the bytes we are filling and then fill in
		 * those bytes with the new fill value.
		 */
		*dstp = (*dstp & keep_mask) | (fill & ~keep_mask);
#else /* __OPTIMIZE_SIZE__ */
		uint8_t *dp, *ep;
		for (dp = (uint8_t *)dstp, ep = dp + len;
		     dp != ep; dp++)
			*dp = fill;
#endif /* __OPTIMIZE_SIZE__ */
	}

	/*
	 * Return the initial addr
	 */
	return addr;
}

#ifdef BZERO
/*
 * For bzero, simply inline memset and let the compiler optimize things away.
 */
void
bzero(void *addr, size_t len)
{
	memset(addr, 0, len);
}
#endif

#ifdef TEST
#include <stdbool.h>
#include <stdio.h>

#undef memset

static union {
	uint8_t bytes[sizeof(memword_t) * 4];
	memword_t words[4];
} testmem;

int
main(int argc, char **argv)
{
	size_t start;
	size_t len;
	bool failed = false;

	for (start = 1; start < sizeof(testmem) - 1; start++) {
		for (len = 1; start + len < sizeof(testmem) - 1; len++) {
			bool ok = true;
			size_t i;
			uint8_t check_value;
			memset(testmem.bytes, 0xff, sizeof(testmem));
			test_memset(testmem.bytes + start, 0x00, len);
			for (i = 0; i < sizeof(testmem); i++) {
				if (i == 0 || i == start + len)
					check_value = 0xff;
				else if (i == start)
					check_value = 0x00;
				if (testmem.bytes[i] != check_value) {
					if (ok)
						printf("pass @ %zu .. %zu failed",
						    start, start + len - 1);
					ok = false;
					printf(" [%zu]=0x%02x(!0x%02x)",
					    i, testmem.bytes[i], check_value);
				}
			}
			if (!ok) {
				printf("\n");
				failed = 1;
			}
		}
	}

	return failed ? 1 : 0;
}
#endif /* TEST */
