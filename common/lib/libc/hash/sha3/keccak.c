/*	$NetBSD: keccak.c,v 1.1 2017/11/30 05:47:24 riastradh Exp $	*/

/*-
 * Copyright (c) 2015 Taylor R. Campbell
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

#include <sys/cdefs.h>

#if defined(_KERNEL) || defined(_STANDALONE)
__KERNEL_RCSID(0, "$NetBSD: keccak.c,v 1.1 2017/11/30 05:47:24 riastradh Exp $");

#include <sys/types.h>
#else
__RCSID("$NetBSD: keccak.c,v 1.1 2017/11/30 05:47:24 riastradh Exp $");

#include <stdint.h>
#endif

#include "keccak.h"

#define	secret	/* can't use in variable-time operations, should zero */

#define	FOR5(X, STMT) do						      \
{									      \
	(X) = 0; STMT;							      \
	(X) = 1; STMT;							      \
	(X) = 2; STMT;							      \
	(X) = 3; STMT;							      \
	(X) = 4; STMT;							      \
} while (0)

static inline secret uint64_t
rol64(secret uint64_t v, unsigned c)
{

	return ((v << c) | (v >> (64 - c)));
}

static inline void
keccakf1600_theta(secret uint64_t A[25])
{
	secret uint64_t C0, C1, C2, C3, C4;
	unsigned y;

	C0 = C1 = C2 = C3 = C4 = 0;
	FOR5(y, {
		C0 ^= A[0 + 5*y];
		C1 ^= A[1 + 5*y];
		C2 ^= A[2 + 5*y];
		C3 ^= A[3 + 5*y];
		C4 ^= A[4 + 5*y];
	});
	FOR5(y, {
		A[0 + 5*y] ^= C4 ^ rol64(C1, 1);
		A[1 + 5*y] ^= C0 ^ rol64(C2, 1);
		A[2 + 5*y] ^= C1 ^ rol64(C3, 1);
		A[3 + 5*y] ^= C2 ^ rol64(C4, 1);
		A[4 + 5*y] ^= C3 ^ rol64(C0, 1);
	});
}

static inline void
keccakf1600_rho_pi(secret uint64_t A[25])
{
	secret uint64_t T, U;

	/*
	 * Permute by (x,y) |---> (y, 2x + 3y mod 5) starting at (1,0),
	 * rotate the ith element by (i + 1)(i + 2)/2 mod 64.
	 */
	U = A[ 1];                       T = U;
	U = A[10]; A[10] = rol64(T,  1); T = U;
	U = A[ 7]; A[ 7] = rol64(T,  3); T = U;
	U = A[11]; A[11] = rol64(T,  6); T = U;
	U = A[17]; A[17] = rol64(T, 10); T = U;
	U = A[18]; A[18] = rol64(T, 15); T = U;
	U = A[ 3]; A[ 3] = rol64(T, 21); T = U;
	U = A[ 5]; A[ 5] = rol64(T, 28); T = U;
	U = A[16]; A[16] = rol64(T, 36); T = U;
	U = A[ 8]; A[ 8] = rol64(T, 45); T = U;
	U = A[21]; A[21] = rol64(T, 55); T = U;
	U = A[24]; A[24] = rol64(T,  2); T = U;
	U = A[ 4]; A[ 4] = rol64(T, 14); T = U;
	U = A[15]; A[15] = rol64(T, 27); T = U;
	U = A[23]; A[23] = rol64(T, 41); T = U;
	U = A[19]; A[19] = rol64(T, 56); T = U;
	U = A[13]; A[13] = rol64(T,  8); T = U;
	U = A[12]; A[12] = rol64(T, 25); T = U;
	U = A[ 2]; A[ 2] = rol64(T, 43); T = U;
	U = A[20]; A[20] = rol64(T, 62); T = U;
	U = A[14]; A[14] = rol64(T, 18); T = U;
	U = A[22]; A[22] = rol64(T, 39); T = U;
	U = A[ 9]; A[ 9] = rol64(T, 61); T = U;
	U = A[ 6]; A[ 6] = rol64(T, 20); T = U;
	           A[ 1] = rol64(T, 44);
}

static inline void
keccakf1600_chi(secret uint64_t A[25])
{
	secret uint64_t B0, B1, B2, B3, B4;
	unsigned y;

	FOR5(y, {
		B0 = A[0 + 5*y];
		B1 = A[1 + 5*y];
		B2 = A[2 + 5*y];
		B3 = A[3 + 5*y];
		B4 = A[4 + 5*y];
		A[0 + 5*y] ^= ~B1 & B2;
		A[1 + 5*y] ^= ~B2 & B3;
		A[2 + 5*y] ^= ~B3 & B4;
		A[3 + 5*y] ^= ~B4 & B0;
		A[4 + 5*y] ^= ~B0 & B1;
	});
}

static void
keccakf1600_round(secret uint64_t A[25])
{

	keccakf1600_theta(A);
	keccakf1600_rho_pi(A);
	keccakf1600_chi(A);
}

void
keccakf1600(secret uint64_t A[25])
{
	/*
	 * RC[i] = \sum_{j = 0,...,6} rc(j + 7i) 2^(2^j - 1),
	 * rc(t) = (x^t mod x^8 + x^6 + x^5 + x^4 + 1) mod x in GF(2)[x]
	 */
	static const uint64_t RC[24] = {
		0x0000000000000001ULL,
		0x0000000000008082ULL,
		0x800000000000808aULL,
		0x8000000080008000ULL,
		0x000000000000808bULL,
		0x0000000080000001ULL,
		0x8000000080008081ULL,
		0x8000000000008009ULL,
		0x000000000000008aULL,
		0x0000000000000088ULL,
		0x0000000080008009ULL,
		0x000000008000000aULL,
		0x000000008000808bULL,
		0x800000000000008bULL,
		0x8000000000008089ULL,
		0x8000000000008003ULL,
		0x8000000000008002ULL,
		0x8000000000000080ULL,
		0x000000000000800aULL,
		0x800000008000000aULL,
		0x8000000080008081ULL,
		0x8000000000008080ULL,
		0x0000000080000001ULL,
		0x8000000080008008ULL,
	};
	unsigned i;

	for (i = 0; i < 24; i++) {
		keccakf1600_round(A);
		A[0] ^= RC[i];
	}
}
