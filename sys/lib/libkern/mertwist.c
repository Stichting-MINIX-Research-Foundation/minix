/*	$NetBSD: mertwist.c,v 1.8 2008/04/28 20:24:06 martin Exp $	*/
/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

#if defined(_KERNEL) || defined(_STANDALONE)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <lib/libkern/libkern.h>
#else
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#define	KASSERT(x)	assert(x)
#endif

/*
 * Mersenne Twister.  Produces identical output compared to mt19937ar.c
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 */

#define	MATRIX_A(a)		(((a) & 1) ? 0x9908b0df : 0)
#define	TEMPERING_MASK_B	0x9d2c5680
#define	TEMPERING_MASK_C	0xefc60000
#define	UPPER_MASK		0x80000000
#define	LOWER_MASK		0x7fffffff
#define	MIX(u,l)		(((u) & UPPER_MASK) | ((l) & LOWER_MASK))

#define	KNUTH_MULTIPLIER	0x6c078965

#ifndef MTPRNG_RLEN
#define	MTPRNG_RLEN		624
#endif
#define	MTPRNG_POS1		397

static void mtprng_refresh(struct mtprng_state *);

/*
 * Initialize the generator from a seed
 */
void
mtprng_init32(struct mtprng_state *mt, uint32_t seed)
{
	size_t i;

	/*
	 * Use Knuth's algorithm for expanding this seed over its
	 * portion of the key space.
	 */
	mt->mt_elem[0] = seed;
	for (i = 1; i < MTPRNG_RLEN; i++) {
		mt->mt_elem[i] = KNUTH_MULTIPLIER
		    * (mt->mt_elem[i-1] ^ (mt->mt_elem[i-1] >> 30)) + i;
	}

	mtprng_refresh(mt);
}

void
mtprng_initarray(struct mtprng_state *mt, const uint32_t *key, size_t keylen)
{
	uint32_t *mp;
	size_t i, j, k;

	/*
	 * Use Knuth's algorithm for expanding this seed over its
	 * portion of the key space.
	 */
	mt->mt_elem[0] = 19650218UL;
	for (i = 1; i < MTPRNG_RLEN; i++) {
		mt->mt_elem[i] = KNUTH_MULTIPLIER
		    * (mt->mt_elem[i-1] ^ (mt->mt_elem[i-1] >> 30)) + i;
	}

	KASSERT(keylen > 0);

	i = 1;
	j = 0;
	k = (keylen < MTPRNG_RLEN ? MTPRNG_RLEN : keylen);

	mp = &mt->mt_elem[1];
	for (; k-- > 0; mp++) {
		mp[0] ^= (mp[-1] ^ (mp[-1] >> 30)) * 1664525UL;
		mp[0] += key[j] + j;
		if (++i == MTPRNG_RLEN) {
			KASSERT(mp == mt->mt_elem + MTPRNG_RLEN - 1);
			mt->mt_elem[0] = mp[0];
			i = 1;
			mp = mt->mt_elem;
		}
		if (++j == keylen)
			j = 0;
	}
	for (j = MTPRNG_RLEN; --j > 0; mp++) {
		mp[0] ^= (mp[-1] ^ (mp[-1] >> 30)) * 1566083941UL;
		mp[0] -= i;
		if (++i == MTPRNG_RLEN) {
			KASSERT(mp == mt->mt_elem + MTPRNG_RLEN - 1);
			mt->mt_elem[0] = mp[0];
			i = 1;
			mp = mt->mt_elem;
		}
	}
	mt->mt_elem[0] = 0x80000000;
	mtprng_refresh(mt);
}

/*
 * Generate an array of 624 untempered numbers
 */
void
mtprng_refresh(struct mtprng_state *mt)
{
	uint32_t y;
	size_t i, j;
	/*
	 * The following has been refactored to avoid the need for 'mod 624'
	*/
	for (i = 0, j = MTPRNG_POS1; j < MTPRNG_RLEN; i++, j++) {
		y = MIX(mt->mt_elem[i], mt->mt_elem[i+1]);
		mt->mt_elem[i] = mt->mt_elem[j] ^ (y >> 1) ^ MATRIX_A(y);
	}
	for (j = 0; i < MTPRNG_RLEN - 1; i++, j++) {
		y = MIX(mt->mt_elem[i], mt->mt_elem[i+1]);
		mt->mt_elem[i] = mt->mt_elem[j] ^ (y >> 1) ^ MATRIX_A(y);
	}
	y = MIX(mt->mt_elem[MTPRNG_RLEN - 1], mt->mt_elem[0]);
	mt->mt_elem[MTPRNG_RLEN - 1] =
	    mt->mt_elem[MTPRNG_POS1 - 1] ^ (y >> 1) ^ MATRIX_A(y);
}
 
/*
 * Extract a tempered PRN based on the current index.  Then recompute a
 * new value for the index.  This avoids having to regenerate the array
 * every 624 iterations. We can do this since recomputing only the next
 * element and the [(i + 397) % 624] one.
 */
uint32_t
mtprng_rawrandom(struct mtprng_state *mt)
{
	uint32_t x, y;
	const size_t i = mt->mt_idx;
	size_t j;

	/*
	 * First generate the random value for the current position.
	 */
	x = mt->mt_elem[i];
	x ^= x >> 11;
	x ^= (x << 7) & TEMPERING_MASK_B;
	x ^= (x << 15) & TEMPERING_MASK_C;
	x ^= x >> 18;

	/*
	 * Next recalculate the next sequence for the current position.
	 */
	y = mt->mt_elem[i];
	if (__predict_true(i < MTPRNG_RLEN - 1)) {
		/*
		 * Avoid doing % since it can be expensive.
		 * j = (i + MTPRNG_POS1) % MTPRNG_RLEN;
		 */
		j = i + MTPRNG_POS1;
		if (j >= MTPRNG_RLEN)
			j -= MTPRNG_RLEN;
		mt->mt_idx++;
	} else {
		j = MTPRNG_POS1 - 1;
		mt->mt_idx = 0;
	}
	y = MIX(y, mt->mt_elem[mt->mt_idx]);
	mt->mt_elem[i] = mt->mt_elem[j] ^ (y >> 1) ^ MATRIX_A(y);

	/*
	 * Return the value calculated in the first step.
	 */
	return x;
}

/*
 * This is a non-standard routine which attempts to return a cryptographically
 * strong random number by collapsing 2 32bit values outputed by the twister
 * into one 32bit value.  
 */
uint32_t
mtprng_random(struct mtprng_state *mt)
{
	uint32_t a;

	mt->mt_count = (mt->mt_count + 13) & 31;
	a = mtprng_rawrandom(mt);
	a = (a << mt->mt_count) | (a >> (32 - mt->mt_count));
	return a + mtprng_rawrandom(mt);
}
