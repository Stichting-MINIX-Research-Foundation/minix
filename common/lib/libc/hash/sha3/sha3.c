/*	$NetBSD: sha3.c,v 1.3 2021/07/31 14:36:33 andvar Exp $	*/

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

/*
 * SHA-3: FIPS-202, Permutation-Based Hash and Extendable-Output Functions
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#if defined(_KERNEL) || defined(_STANDALONE)

__KERNEL_RCSID(0, "$NetBSD: sha3.c,v 1.3 2021/07/31 14:36:33 andvar Exp $");
#include <lib/libkern/libkern.h>

#define	SHA3_ASSERT	KASSERT

#else

__RCSID("$NetBSD: sha3.c,v 1.3 2021/07/31 14:36:33 andvar Exp $");

#include "namespace.h"

#include <assert.h>
#include <string.h>

#define	SHA3_ASSERT	_DIAGASSERT

#endif

#include <sys/endian.h>
#include <sys/sha3.h>

#include "keccak.h"

/* XXX Disabled for now -- these will be libc-private.  */
#if 0 && !defined(_KERNEL) && !defined(_STANDALONE)
#ifdef __weak_alias
__weak_alias(SHA3_224_Init,_SHA3_224_Init)
__weak_alias(SHA3_224_Update,_SHA3_224_Update)
__weak_alias(SHA3_224_Final,_SHA3_224_Final)
__weak_alias(SHA3_256_Init,_SHA3_256_Init)
__weak_alias(SHA3_256_Update,_SHA3_256_Update)
__weak_alias(SHA3_256_Final,_SHA3_256_Final)
__weak_alias(SHA3_384_Init,_SHA3_384_Init)
__weak_alias(SHA3_384_Update,_SHA3_384_Update)
__weak_alias(SHA3_384_Final,_SHA3_384_Final)
__weak_alias(SHA3_512_Init,_SHA3_512_Init)
__weak_alias(SHA3_512_Update,_SHA3_512_Update)
__weak_alias(SHA3_512_Final,_SHA3_512_Final)
__weak_alias(SHA3_Selftest,_SHA3_Selftest)
__weak_alias(SHAKE128_Init,_SHAKE128_Init)
__weak_alias(SHAKE128_Update,_SHAKE128_Update)
__weak_alias(SHAKE128_Final,_SHAKE128_Final)
__weak_alias(SHAKE256_Init,_SHAKE256_Init)
__weak_alias(SHAKE256_Update,_SHAKE256_Update)
__weak_alias(SHAKE256_Final,_SHAKE256_Final)
#endif	/* __weak_alias */
#endif	/* kernel/standalone */

#define	MIN(a,b)	((a) < (b) ? (a) : (b))
#define	arraycount(a)	(sizeof(a)/sizeof((a)[0]))

/*
 * Common body.  All the SHA-3 functions share code structure.  They
 * differ only in the size of the chunks they split the message into:
 * for digest size d, they are split into chunks of 200 - d bytes.
 */

static inline unsigned
sha3_rate(unsigned d)
{
	const unsigned cw = 2*d/8;	/* capacity in words */

	return 25 - cw;
}

static void
sha3_init(struct sha3 *C, unsigned rw)
{
	unsigned iw;

	C->nb = 8*rw;
	for (iw = 0; iw < 25; iw++)
		C->A[iw] = 0;
}

static void
sha3_update(struct sha3 *C, const uint8_t *data, size_t len, unsigned rw)
{
	uint64_t T;
	unsigned ib, iw;		/* index of byte/word */

	assert(0 < C->nb);

	/* If there's a partial word, try to fill it.  */
	if ((C->nb % 8) != 0) {
		T = 0;
		for (ib = 0; ib < MIN(len, C->nb % 8); ib++)
			T |= (uint64_t)data[ib] << (8*ib);
		C->A[rw - (C->nb + 7)/8] ^= T << (8*(8 - (C->nb % 8)));
		C->nb -= ib;
		data += ib;
		len -= ib;

		/* If we filled the buffer, permute now.  */
		if (C->nb == 0) {
			keccakf1600(C->A);
			C->nb = 8*rw;
		}

		/* If that exhausted the input, we're done.  */
		if (len == 0)
			return;
	}

	/* At a word boundary.  Fill any partial buffer.  */
	assert((C->nb % 8) == 0);
	if (C->nb < 8*rw) {
		for (iw = 0; iw < MIN(len, C->nb)/8; iw++)
			C->A[rw - C->nb/8 + iw] ^= le64dec(data + 8*iw);
		C->nb -= 8*iw;
		data += 8*iw;
		len -= 8*iw;

		/* If we filled the buffer, permute now.  */
		if (C->nb == 0) {
			keccakf1600(C->A);
			C->nb = 8*rw;
		} else {
			/* Otherwise, less than a word left.  */
			assert(len < 8);
			goto partial;
		}
	}

	/* At a buffer boundary.  Absorb input one buffer at a time.  */
	assert(C->nb == 8*rw);
	while (8*rw <= len) {
		for (iw = 0; iw < rw; iw++)
			C->A[iw] ^= le64dec(data + 8*iw);
		keccakf1600(C->A);
		data += 8*rw;
		len -= 8*rw;
	}

	/* Partially fill the buffer with as many words as we can.  */
	for (iw = 0; iw < len/8; iw++)
		C->A[rw - C->nb/8 + iw] ^= le64dec(data + 8*iw);
	C->nb -= 8*iw;
	data += 8*iw;
	len -= 8*iw;

partial:
	/* Partially fill the last word with as many bytes as we can.  */
	assert(len < 8);
	assert(0 < C->nb);
	assert((C->nb % 8) == 0);
	T = 0;
	for (ib = 0; ib < len; ib++)
		T |= (uint64_t)data[ib] << (8*ib);
	C->A[rw - C->nb/8] ^= T;
	C->nb -= ib;
	assert(0 < C->nb);
}

static void
sha3_final(uint8_t *h, unsigned d, struct sha3 *C, unsigned rw)
{
	unsigned nw, iw;

	assert(d <= 8*25);
	assert(0 < C->nb);

	/* Append 01, pad with 10*1 up to buffer boundary, LSB first.  */
	nw = (C->nb + 7)/8;
	assert(0 < nw);
	assert(nw <= rw);
	C->A[rw - nw] ^= (uint64_t)0x06 << (8*(8*nw - C->nb));
	C->A[rw - 1] ^= 0x8000000000000000ULL;

	/* Permute one last time.  */
	keccakf1600(C->A);

	/* Reveal the first 8d bits of state, forget 1600-8d of them.  */
	for (iw = 0; iw < d/8; iw++)
		le64enc(h + 8*iw, C->A[iw]);
	h += 8*iw;
	d -= 8*iw;
	if (0 < d) {
		/* For SHA3-224, we need to expose a partial word.  */
		uint64_t T = C->A[iw];
		do {
			*h++ = T & 0xff;
			T >>= 8;
		} while (--d);
	}
	(void)explicit_memset(C->A, 0, sizeof C->A);
	C->nb = 0;
}

static void
shake_final(uint8_t *h, unsigned d, struct sha3 *C, unsigned rw)
{
	unsigned nw, iw;

	assert(0 < C->nb);

	/* Append 1111, pad with 10*1 up to buffer boundary, LSB first.  */
	nw = (C->nb + 7)/8;
	assert(0 < nw);
	assert(nw <= rw);
	C->A[rw - nw] ^= (uint64_t)0x1f << (8*(8*nw - C->nb));
	C->A[rw - 1] ^= 0x8000000000000000ULL;

	/* Permute, reveal first rw words of state, repeat.  */
	while (8*rw <= d) {
		keccakf1600(C->A);
		for (iw = 0; iw < rw; iw++)
			le64enc(h + 8*iw, C->A[iw]);
		h += 8*iw;
		d -= 8*iw;
	}

	/*
	 * If 8*rw (the output rate in bytes) does not divide d, more
	 * words are wanted: permute again and reveal a little more.
	 */
	if (0 < d) {
		keccakf1600(C->A);
		for (iw = 0; iw < d/8; iw++)
			le64enc(h + 8*iw, C->A[iw]);
		h += 8*iw;
		d -= 8*iw;

		/*
		 * If 8 does not divide d, more bytes are wanted:
		 * reveal them.
		 */
		if (0 < d) {
			uint64_t T = C->A[iw];
			do {
				*h++ = T & 0xff;
				T >>= 8;
			} while (--d);
		}
	}

	(void)explicit_memset(C->A, 0, sizeof C->A);
	C->nb = 0;
}

void
SHA3_224_Init(SHA3_224_CTX *C)
{

	sha3_init(&C->C224, sha3_rate(SHA3_224_DIGEST_LENGTH));
}

void
SHA3_224_Update(SHA3_224_CTX *C, const uint8_t *data, size_t len)
{

	sha3_update(&C->C224, data, len, sha3_rate(SHA3_224_DIGEST_LENGTH));
}

void
SHA3_224_Final(uint8_t h[SHA3_224_DIGEST_LENGTH], SHA3_224_CTX *C)
{

	sha3_final(h, SHA3_224_DIGEST_LENGTH, &C->C224,
	    sha3_rate(SHA3_224_DIGEST_LENGTH));
}

void
SHA3_256_Init(SHA3_256_CTX *C)
{

	sha3_init(&C->C256, sha3_rate(SHA3_256_DIGEST_LENGTH));
}

void
SHA3_256_Update(SHA3_256_CTX *C, const uint8_t *data, size_t len)
{

	sha3_update(&C->C256, data, len, sha3_rate(SHA3_256_DIGEST_LENGTH));
}

void
SHA3_256_Final(uint8_t h[SHA3_256_DIGEST_LENGTH], SHA3_256_CTX *C)
{

	sha3_final(h, SHA3_256_DIGEST_LENGTH, &C->C256,
	    sha3_rate(SHA3_256_DIGEST_LENGTH));
}

void
SHA3_384_Init(SHA3_384_CTX *C)
{

	sha3_init(&C->C384, sha3_rate(SHA3_384_DIGEST_LENGTH));
}

void
SHA3_384_Update(SHA3_384_CTX *C, const uint8_t *data, size_t len)
{

	sha3_update(&C->C384, data, len, sha3_rate(SHA3_384_DIGEST_LENGTH));
}

void
SHA3_384_Final(uint8_t h[SHA3_384_DIGEST_LENGTH], SHA3_384_CTX *C)
{

	sha3_final(h, SHA3_384_DIGEST_LENGTH, &C->C384,
	    sha3_rate(SHA3_384_DIGEST_LENGTH));
}

void
SHA3_512_Init(SHA3_512_CTX *C)
{

	sha3_init(&C->C512, sha3_rate(SHA3_512_DIGEST_LENGTH));
}

void
SHA3_512_Update(SHA3_512_CTX *C, const uint8_t *data, size_t len)
{

	sha3_update(&C->C512, data, len, sha3_rate(SHA3_512_DIGEST_LENGTH));
}

void
SHA3_512_Final(uint8_t h[SHA3_512_DIGEST_LENGTH], SHA3_512_CTX *C)
{

	sha3_final(h, SHA3_512_DIGEST_LENGTH, &C->C512,
	    sha3_rate(SHA3_512_DIGEST_LENGTH));
}

void
SHAKE128_Init(SHAKE128_CTX *C)
{

	sha3_init(&C->C128, sha3_rate(128/8));
}

void
SHAKE128_Update(SHAKE128_CTX *C, const uint8_t *data, size_t len)
{

	sha3_update(&C->C128, data, len, sha3_rate(128/8));
}

void
SHAKE128_Final(uint8_t *h, size_t d, SHAKE128_CTX *C)
{

	shake_final(h, d, &C->C128, sha3_rate(128/8));
}

void
SHAKE256_Init(SHAKE256_CTX *C)
{

	sha3_init(&C->C256, sha3_rate(256/8));
}

void
SHAKE256_Update(SHAKE256_CTX *C, const uint8_t *data, size_t len)
{

	sha3_update(&C->C256, data, len, sha3_rate(256/8));
}

void
SHAKE256_Final(uint8_t *h, size_t d, SHAKE256_CTX *C)
{

	shake_final(h, d, &C->C256, sha3_rate(256/8));
}

static void
sha3_selftest_prng(void *buf, size_t len, uint32_t seed)
{
	uint8_t *p = buf;
	size_t n = len;
	uint32_t t, a, b;

	a = 0xdead4bad * seed;
	b = 1;

	while (n--) {
		t = a + b;
		*p++ = t >> 24;
		a = b;
		b = t;
	}
}

int
SHA3_Selftest(void)
{
	static const uint8_t d224_0[] = { /* SHA3-224(0-bit) */
		0x6b,0x4e,0x03,0x42,0x36,0x67,0xdb,0xb7,
		0x3b,0x6e,0x15,0x45,0x4f,0x0e,0xb1,0xab,
		0xd4,0x59,0x7f,0x9a,0x1b,0x07,0x8e,0x3f,
		0x5b,0x5a,0x6b,0xc7,
	};
	static const uint8_t d256_0[] = { /* SHA3-256(0-bit) */
		0xa7,0xff,0xc6,0xf8,0xbf,0x1e,0xd7,0x66,
		0x51,0xc1,0x47,0x56,0xa0,0x61,0xd6,0x62,
		0xf5,0x80,0xff,0x4d,0xe4,0x3b,0x49,0xfa,
		0x82,0xd8,0x0a,0x4b,0x80,0xf8,0x43,0x4a,
	};
	static const uint8_t d384_0[] = { /* SHA3-384(0-bit) */
		0x0c,0x63,0xa7,0x5b,0x84,0x5e,0x4f,0x7d,
		0x01,0x10,0x7d,0x85,0x2e,0x4c,0x24,0x85,
		0xc5,0x1a,0x50,0xaa,0xaa,0x94,0xfc,0x61,
		0x99,0x5e,0x71,0xbb,0xee,0x98,0x3a,0x2a,
		0xc3,0x71,0x38,0x31,0x26,0x4a,0xdb,0x47,
		0xfb,0x6b,0xd1,0xe0,0x58,0xd5,0xf0,0x04,
	};
	static const uint8_t d512_0[] = { /* SHA3-512(0-bit) */
		0xa6,0x9f,0x73,0xcc,0xa2,0x3a,0x9a,0xc5,
		0xc8,0xb5,0x67,0xdc,0x18,0x5a,0x75,0x6e,
		0x97,0xc9,0x82,0x16,0x4f,0xe2,0x58,0x59,
		0xe0,0xd1,0xdc,0xc1,0x47,0x5c,0x80,0xa6,
		0x15,0xb2,0x12,0x3a,0xf1,0xf5,0xf9,0x4c,
		0x11,0xe3,0xe9,0x40,0x2c,0x3a,0xc5,0x58,
		0xf5,0x00,0x19,0x9d,0x95,0xb6,0xd3,0xe3,
		0x01,0x75,0x85,0x86,0x28,0x1d,0xcd,0x26,
	};
	static const uint8_t shake128_0_41[] = { /* SHAKE128(0-bit, 41) */
		0x7f,0x9c,0x2b,0xa4,0xe8,0x8f,0x82,0x7d,
		0x61,0x60,0x45,0x50,0x76,0x05,0x85,0x3e,
		0xd7,0x3b,0x80,0x93,0xf6,0xef,0xbc,0x88,
		0xeb,0x1a,0x6e,0xac,0xfa,0x66,0xef,0x26,
		0x3c,0xb1,0xee,0xa9,0x88,0x00,0x4b,0x93,0x10,
	};
	static const uint8_t shake256_0_73[] = { /* SHAKE256(0-bit, 73) */
		0x46,0xb9,0xdd,0x2b,0x0b,0xa8,0x8d,0x13,
		0x23,0x3b,0x3f,0xeb,0x74,0x3e,0xeb,0x24,
		0x3f,0xcd,0x52,0xea,0x62,0xb8,0x1b,0x82,
		0xb5,0x0c,0x27,0x64,0x6e,0xd5,0x76,0x2f,
		0xd7,0x5d,0xc4,0xdd,0xd8,0xc0,0xf2,0x00,
		0xcb,0x05,0x01,0x9d,0x67,0xb5,0x92,0xf6,
		0xfc,0x82,0x1c,0x49,0x47,0x9a,0xb4,0x86,
		0x40,0x29,0x2e,0xac,0xb3,0xb7,0xc4,0xbe,
		0x14,0x1e,0x96,0x61,0x6f,0xb1,0x39,0x57,0x69,
	};
	static const uint8_t d224_1600[] = { /* SHA3-224(200 * 0xa3) */
		0x93,0x76,0x81,0x6a,0xba,0x50,0x3f,0x72,
		0xf9,0x6c,0xe7,0xeb,0x65,0xac,0x09,0x5d,
		0xee,0xe3,0xbe,0x4b,0xf9,0xbb,0xc2,0xa1,
		0xcb,0x7e,0x11,0xe0,
	};
	static const uint8_t d256_1600[] = { /* SHA3-256(200 * 0xa3) */
		0x79,0xf3,0x8a,0xde,0xc5,0xc2,0x03,0x07,
		0xa9,0x8e,0xf7,0x6e,0x83,0x24,0xaf,0xbf,
		0xd4,0x6c,0xfd,0x81,0xb2,0x2e,0x39,0x73,
		0xc6,0x5f,0xa1,0xbd,0x9d,0xe3,0x17,0x87,
	};
	static const uint8_t d384_1600[] = { /* SHA3-384(200 * 0xa3) */
		0x18,0x81,0xde,0x2c,0xa7,0xe4,0x1e,0xf9,
		0x5d,0xc4,0x73,0x2b,0x8f,0x5f,0x00,0x2b,
		0x18,0x9c,0xc1,0xe4,0x2b,0x74,0x16,0x8e,
		0xd1,0x73,0x26,0x49,0xce,0x1d,0xbc,0xdd,
		0x76,0x19,0x7a,0x31,0xfd,0x55,0xee,0x98,
		0x9f,0x2d,0x70,0x50,0xdd,0x47,0x3e,0x8f,
	};
	static const uint8_t d512_1600[] = { /* SHA3-512(200 * 0xa3) */
		0xe7,0x6d,0xfa,0xd2,0x20,0x84,0xa8,0xb1,
		0x46,0x7f,0xcf,0x2f,0xfa,0x58,0x36,0x1b,
		0xec,0x76,0x28,0xed,0xf5,0xf3,0xfd,0xc0,
		0xe4,0x80,0x5d,0xc4,0x8c,0xae,0xec,0xa8,
		0x1b,0x7c,0x13,0xc3,0x0a,0xdf,0x52,0xa3,
		0x65,0x95,0x84,0x73,0x9a,0x2d,0xf4,0x6b,
		0xe5,0x89,0xc5,0x1c,0xa1,0xa4,0xa8,0x41,
		0x6d,0xf6,0x54,0x5a,0x1c,0xe8,0xba,0x00,
	};
	static const uint8_t shake128_1600_41[] = {
		/* SHAKE128(200 * 0xa3, 41) */
		0x13,0x1a,0xb8,0xd2,0xb5,0x94,0x94,0x6b,
		0x9c,0x81,0x33,0x3f,0x9b,0xb6,0xe0,0xce,
		0x75,0xc3,0xb9,0x31,0x04,0xfa,0x34,0x69,
		0xd3,0x91,0x74,0x57,0x38,0x5d,0xa0,0x37,
		0xcf,0x23,0x2e,0xf7,0x16,0x4a,0x6d,0x1e,0xb4,
	};
	static const uint8_t shake256_1600_73[] = {
		/* SHAKE256(200 * 0xa3, 73) */
		0xcd,0x8a,0x92,0x0e,0xd1,0x41,0xaa,0x04,
		0x07,0xa2,0x2d,0x59,0x28,0x86,0x52,0xe9,
		0xd9,0xf1,0xa7,0xee,0x0c,0x1e,0x7c,0x1c,
		0xa6,0x99,0x42,0x4d,0xa8,0x4a,0x90,0x4d,
		0x2d,0x70,0x0c,0xaa,0xe7,0x39,0x6e,0xce,
		0x96,0x60,0x44,0x40,0x57,0x7d,0xa4,0xf3,
		0xaa,0x22,0xae,0xb8,0x85,0x7f,0x96,0x1c,
		0x4c,0xd8,0xe0,0x6f,0x0a,0xe6,0x61,0x0b,
		0x10,0x48,0xa7,0xf6,0x4e,0x10,0x74,0xcd,0x62,
	};
	static const uint8_t d0[] = {
		0x5d,0x3e,0x45,0xdd,0x9b,0x6b,0xda,0xf8,
		0xe6,0xe6,0xb8,0x72,0xfb,0xc5,0x0d,0x0a,
		0x4f,0x52,0x65,0xb4,0x11,0xf1,0xa1,0x0c,
		0x00,0xa4,0x74,0x6c,0x0f,0xc0,0xdc,0xe0,
		0x97,0x73,0xd6,0x70,0xaf,0xd4,0x64,0x0b,
		0x8c,0x52,0x32,0x4c,0x87,0x8c,0xfa,0x4a,
		0xdc,0x11,0x66,0x91,0x66,0x5a,0x1e,0xa4,
		0xd6,0x69,0x97,0xc7,0xcb,0xe2,0x73,0xca,
	};
	static const unsigned mlen[] = { 0, 3, 128, 129, 255 };
	uint8_t m[255], d[73];
	struct sha3 sha3;
	SHA3_224_CTX *sha3224 = (SHA3_224_CTX *)&sha3;
	SHA3_256_CTX *sha3256 = (SHA3_256_CTX *)&sha3;
	SHA3_384_CTX *sha3384 = (SHA3_384_CTX *)&sha3;
	SHA3_512_CTX *sha3512 = (SHA3_512_CTX *)&sha3;
	SHAKE128_CTX *shake128 = (SHAKE128_CTX *)&sha3;
	SHAKE256_CTX *shake256 = (SHAKE256_CTX *)&sha3;
	SHA3_512_CTX ctx;
	unsigned mi;

	/*
	 * NIST test vectors from
	 * <http://csrc.nist.gov/groups/ST/toolkit/examples.html#aHashing>:
	 * 0-bit, 1600-bit repeated 0xa3 (= 0b10100011).
	 */
	SHA3_224_Init(sha3224);
	SHA3_224_Final(d, sha3224);
	if (memcmp(d, d224_0, 28) != 0)
		return -1;
	SHA3_256_Init(sha3256);
	SHA3_256_Final(d, sha3256);
	if (memcmp(d, d256_0, 32) != 0)
		return -1;
	SHA3_384_Init(sha3384);
	SHA3_384_Final(d, sha3384);
	if (memcmp(d, d384_0, 48) != 0)
		return -1;
	SHA3_512_Init(sha3512);
	SHA3_512_Final(d, sha3512);
	if (memcmp(d, d512_0, 64) != 0)
		return -1;
	SHAKE128_Init(shake128);
	SHAKE128_Final(d, 41, shake128);
	if (memcmp(d, shake128_0_41, 41) != 0)
		return -1;
	SHAKE256_Init(shake256);
	SHAKE256_Final(d, 73, shake256);
	if (memcmp(d, shake256_0_73, 73) != 0)
		return -1;

	(void)memset(m, 0xa3, 200);
	SHA3_224_Init(sha3224);
	SHA3_224_Update(sha3224, m, 200);
	SHA3_224_Final(d, sha3224);
	if (memcmp(d, d224_1600, 28) != 0)
		return -1;
	SHA3_256_Init(sha3256);
	SHA3_256_Update(sha3256, m, 200);
	SHA3_256_Final(d, sha3256);
	if (memcmp(d, d256_1600, 32) != 0)
		return -1;
	SHA3_384_Init(sha3384);
	SHA3_384_Update(sha3384, m, 200);
	SHA3_384_Final(d, sha3384);
	if (memcmp(d, d384_1600, 48) != 0)
		return -1;
	SHA3_512_Init(sha3512);
	SHA3_512_Update(sha3512, m, 200);
	SHA3_512_Final(d, sha3512);
	if (memcmp(d, d512_1600, 64) != 0)
		return -1;
	SHAKE128_Init(shake128);
	SHAKE128_Update(shake128, m, 200);
	SHAKE128_Final(d, 41, shake128);
	if (memcmp(d, shake128_1600_41, 41) != 0)
		return -1;
	SHAKE256_Init(shake256);
	SHAKE256_Update(shake256, m, 200);
	SHAKE256_Final(d, 73, shake256);
	if (memcmp(d, shake256_1600_73, 73) != 0)
		return -1;

	/*
	 * Hand-crufted test vectors with unaligned message lengths.
	 */
	SHA3_512_Init(&ctx);
	for (mi = 0; mi < arraycount(mlen); mi++) {
		sha3_selftest_prng(m, mlen[mi], (224/8)*mlen[mi]);
		SHA3_224_Init(sha3224);
		SHA3_224_Update(sha3224, m, mlen[mi]);
		SHA3_224_Final(d, sha3224);
		SHA3_512_Update(&ctx, d, 224/8);
	}
	for (mi = 0; mi < arraycount(mlen); mi++) {
		sha3_selftest_prng(m, mlen[mi], (256/8)*mlen[mi]);
		SHA3_256_Init(sha3256);
		SHA3_256_Update(sha3256, m, mlen[mi]);
		SHA3_256_Final(d, sha3256);
		SHA3_512_Update(&ctx, d, 256/8);
	}
	for (mi = 0; mi < arraycount(mlen); mi++) {
		sha3_selftest_prng(m, mlen[mi], (384/8)*mlen[mi]);
		SHA3_384_Init(sha3384);
		SHA3_384_Update(sha3384, m, mlen[mi]);
		SHA3_384_Final(d, sha3384);
		SHA3_512_Update(&ctx, d, 384/8);
	}
	for (mi = 0; mi < arraycount(mlen); mi++) {
		sha3_selftest_prng(m, mlen[mi], (512/8)*mlen[mi]);
		SHA3_512_Init(sha3512);
		SHA3_512_Update(sha3512, m, mlen[mi]);
		SHA3_512_Final(d, sha3512);
		SHA3_512_Update(&ctx, d, 512/8);
	}
	SHA3_512_Final(d, &ctx);
	if (memcmp(d, d0, 64) != 0)
		return -1;

	return 0;
}
