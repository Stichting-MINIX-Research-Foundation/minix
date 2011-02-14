/* $NetBSD: sha2.c,v 1.21 2010/01/24 21:11:18 joerg Exp $ */
/*	$KAME: sha2.c,v 1.9 2003/07/20 00:28:38 itojun Exp $	*/

/*
 * sha2.c
 *
 * Version 1.0.0beta1
 *
 * Written by Aaron D. Gifford <me@aarongifford.com>
 *
 * Copyright 2000 Aaron D. Gifford.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#if defined(_KERNEL) || defined(_STANDALONE)
__KERNEL_RCSID(0, "$NetBSD: sha2.c,v 1.21 2010/01/24 21:11:18 joerg Exp $");

#include <sys/param.h>	/* XXX: to pull <machine/macros.h> for vax memset(9) */
#include <lib/libkern/libkern.h>

#else

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sha2.c,v 1.21 2010/01/24 21:11:18 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <string.h>

#endif

#include <sys/types.h>
#include <sys/sha2.h>

#if HAVE_NBTOOL_CONFIG_H
#  if HAVE_SYS_ENDIAN_H
#    include <sys/endian.h>
#  else
#   undef htobe32
#   undef htobe64
#   undef be32toh
#   undef be64toh

static uint32_t
htobe32(uint32_t x)
{
	uint8_t p[4];
	memcpy(p, &x, 4);

	return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static uint64_t
htobe64(uint64_t x)
{
	uint8_t p[8];
	uint32_t u, v;
	memcpy(p, &x, 8);

	u = ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
	v = ((p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7]);

	return ((((uint64_t)u) << 32) | v);
}

static uint32_t
be32toh(uint32_t x)
{
	return htobe32(x);
}

static uint64_t
be64toh(uint64_t x)
{
	return htobe64(x);
}
#  endif
#endif

/*** SHA-256/384/512 Various Length Definitions ***********************/
/* NOTE: Most of these are in sha2.h */
#define SHA256_SHORT_BLOCK_LENGTH	(SHA256_BLOCK_LENGTH - 8)
#define SHA384_SHORT_BLOCK_LENGTH	(SHA384_BLOCK_LENGTH - 16)
#define SHA512_SHORT_BLOCK_LENGTH	(SHA512_BLOCK_LENGTH - 16)

/*
 * Macro for incrementally adding the unsigned 64-bit integer n to the
 * unsigned 128-bit integer (represented using a two-element array of
 * 64-bit words):
 */
#define ADDINC128(w,n)	{ \
	(w)[0] += (uint64_t)(n); \
	if ((w)[0] < (n)) { \
		(w)[1]++; \
	} \
}

/*** THE SIX LOGICAL FUNCTIONS ****************************************/
/*
 * Bit shifting and rotation (used by the six SHA-XYZ logical functions:
 *
 *   NOTE:  The naming of R and S appears backwards here (R is a SHIFT and
 *   S is a ROTATION) because the SHA-256/384/512 description document
 *   (see http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf) uses this
 *   same "backwards" definition.
 */
/* Shift-right (used in SHA-256, SHA-384, and SHA-512): */
#define R(b,x) 		((x) >> (b))
/* 32-bit Rotate-right (used in SHA-256): */
#define S32(b,x)	(((x) >> (b)) | ((x) << (32 - (b))))
/* 64-bit Rotate-right (used in SHA-384 and SHA-512): */
#define S64(b,x)	(((x) >> (b)) | ((x) << (64 - (b))))

/* Two of six logical functions used in SHA-256, SHA-384, and SHA-512: */
#define Ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* Four of six logical functions used in SHA-256: */
#define Sigma0_256(x)	(S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))
#define Sigma1_256(x)	(S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
#define sigma0_256(x)	(S32(7,  (x)) ^ S32(18, (x)) ^ R(3 ,   (x)))
#define sigma1_256(x)	(S32(17, (x)) ^ S32(19, (x)) ^ R(10,   (x)))

/* Four of six logical functions used in SHA-384 and SHA-512: */
#define Sigma0_512(x)	(S64(28, (x)) ^ S64(34, (x)) ^ S64(39, (x)))
#define Sigma1_512(x)	(S64(14, (x)) ^ S64(18, (x)) ^ S64(41, (x)))
#define sigma0_512(x)	(S64( 1, (x)) ^ S64( 8, (x)) ^ R( 7,   (x)))
#define sigma1_512(x)	(S64(19, (x)) ^ S64(61, (x)) ^ R( 6,   (x)))

/*** INTERNAL FUNCTION PROTOTYPES *************************************/
/* NOTE: These should not be accessed directly from outside this
 * library -- they are intended for private internal visibility/use
 * only.
 */
static void SHA512_Last(SHA512_CTX *);
void SHA224_Transform(SHA224_CTX *, const uint32_t*);
void SHA256_Transform(SHA256_CTX *, const uint32_t*);
void SHA384_Transform(SHA384_CTX *, const uint64_t*);
void SHA512_Transform(SHA512_CTX *, const uint64_t*);


/*** SHA-XYZ INITIAL HASH VALUES AND CONSTANTS ************************/
/* Hash constant words K for SHA-256: */
static const uint32_t K256[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

/* Initial hash value H for SHA-224: */
static const uint32_t sha224_initial_hash_value[8] = {
	0xc1059ed8UL,
	0x367cd507UL,
	0x3070dd17UL,
	0xf70e5939UL,
	0xffc00b31UL,
	0x68581511UL,
	0x64f98fa7UL,
	0xbefa4fa4UL
};

/* Initial hash value H for SHA-256: */
static const uint32_t sha256_initial_hash_value[8] = {
	0x6a09e667UL,
	0xbb67ae85UL,
	0x3c6ef372UL,
	0xa54ff53aUL,
	0x510e527fUL,
	0x9b05688cUL,
	0x1f83d9abUL,
	0x5be0cd19UL
};

/* Hash constant words K for SHA-384 and SHA-512: */
static const uint64_t K512[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/* Initial hash value H for SHA-384 */
static const uint64_t sha384_initial_hash_value[8] = {
	0xcbbb9d5dc1059ed8ULL,
	0x629a292a367cd507ULL,
	0x9159015a3070dd17ULL,
	0x152fecd8f70e5939ULL,
	0x67332667ffc00b31ULL,
	0x8eb44a8768581511ULL,
	0xdb0c2e0d64f98fa7ULL,
	0x47b5481dbefa4fa4ULL
};

/* Initial hash value H for SHA-512 */
static const uint64_t sha512_initial_hash_value[8] = {
	0x6a09e667f3bcc908ULL,
	0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL,
	0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL,
	0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL,
	0x5be0cd19137e2179ULL
};

#if !defined(_KERNEL) && !defined(_STANDALONE)
#if defined(__weak_alias)
__weak_alias(SHA224_Init,_SHA224_Init) 
__weak_alias(SHA224_Update,_SHA224_Update)
__weak_alias(SHA224_Final,_SHA224_Final)
__weak_alias(SHA224_Transform,_SHA224_Transform)

__weak_alias(SHA256_Init,_SHA256_Init) 
__weak_alias(SHA256_Update,_SHA256_Update)
__weak_alias(SHA256_Final,_SHA256_Final)
__weak_alias(SHA256_Transform,_SHA256_Transform)

__weak_alias(SHA384_Init,_SHA384_Init) 
__weak_alias(SHA384_Update,_SHA384_Update)
__weak_alias(SHA384_Final,_SHA384_Final)
__weak_alias(SHA384_Transform,_SHA384_Transform)

__weak_alias(SHA512_Init,_SHA512_Init) 
__weak_alias(SHA512_Update,_SHA512_Update)
__weak_alias(SHA512_Final,_SHA512_Final)
__weak_alias(SHA512_Transform,_SHA512_Transform)
#endif
#endif

/*** SHA-256: *********************************************************/
int
SHA256_Init(SHA256_CTX *context)
{
	if (context == NULL)
		return 1;

	memcpy(context->state, sha256_initial_hash_value,
	    (size_t)(SHA256_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA256_BLOCK_LENGTH));
	context->bitcount = 0;

	return 1;
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-256 round macros: */

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	W256[j] = be32toh(*data);		\
	++data;					\
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
             K256[j] + W256[j]; \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

#define ROUND256(a,b,c,d,e,f,g,h)	\
	s0 = W256[(j+1)&0x0f]; \
	s0 = sigma0_256(s0); \
	s1 = W256[(j+14)&0x0f]; \
	s1 = sigma1_256(s1); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + K256[j] + \
	     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

void 
SHA256_Transform(SHA256_CTX *context, const uint32_t *data)
{
	uint32_t	a, b, c, d, e, f, g, h, s0, s1;
	uint32_t	T1, *W256;
	int		j;

	W256 = (uint32_t *)context->buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		/* Rounds 0 to 15 (unrolled): */
		ROUND256_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND256_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND256_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND256_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND256_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND256_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND256_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND256_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds to 64: */
	do {
		ROUND256(a,b,c,d,e,f,g,h);
		ROUND256(h,a,b,c,d,e,f,g);
		ROUND256(g,h,a,b,c,d,e,f);
		ROUND256(f,g,h,a,b,c,d,e);
		ROUND256(e,f,g,h,a,b,c,d);
		ROUND256(d,e,f,g,h,a,b,c);
		ROUND256(c,d,e,f,g,h,a,b);
		ROUND256(b,c,d,e,f,g,h,a);
	} while (j < 64);

	/* Compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* SHA2_UNROLL_TRANSFORM */

void
SHA256_Transform(SHA256_CTX *context, const uint32_t *data)
{
	uint32_t	a, b, c, d, e, f, g, h, s0, s1;
	uint32_t	T1, T2, *W256;
	int		j;

	W256 = (uint32_t *)(void *)context->buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		W256[j] = be32toh(*data);
		++data;
		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + W256[j];
		T2 = Sigma0_256(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W256[(j+1)&0x0f];
		s0 = sigma0_256(s0);
		s1 = W256[(j+14)&0x0f];
		s1 = sigma1_256(s1);

		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] +
		     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0);
		T2 = Sigma0_256(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 64);

	/* Compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

int
SHA256_Update(SHA256_CTX *context, const uint8_t *data, size_t len)
{
	unsigned int	freespace, usedspace;

	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return 1;
	}

	usedspace = (unsigned int)((context->bitcount >> 3) %
				    SHA256_BLOCK_LENGTH);
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = SHA256_BLOCK_LENGTH - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			memcpy(&context->buffer[usedspace], data,
			    (size_t)(freespace));
			context->bitcount += freespace << 3;
			len -= freespace;
			data += freespace;
			SHA256_Transform(context,
			    (uint32_t *)(void *)context->buffer);
		} else {
			/* The buffer is not yet full */
			memcpy(&context->buffer[usedspace], data, len);
			context->bitcount += len << 3;
			/* Clean up: */
			usedspace = freespace = 0;
			return 1;
		}
	}
	/*
	 * Process as many complete blocks as possible.
	 *
	 * Check alignment of the data pointer. If it is 32bit aligned,
	 * SHA256_Transform can be called directly on the data stream,
	 * otherwise enforce the alignment by copy into the buffer.
	 */
	if ((uintptr_t)data % 4 == 0) {
		while (len >= SHA256_BLOCK_LENGTH) {
			SHA256_Transform(context,
			    (const uint32_t *)(const void *)data);
			context->bitcount += SHA256_BLOCK_LENGTH << 3;
			len -= SHA256_BLOCK_LENGTH;
			data += SHA256_BLOCK_LENGTH;
		}
	} else {
		while (len >= SHA256_BLOCK_LENGTH) {
			memcpy(context->buffer, data, SHA256_BLOCK_LENGTH);
			SHA256_Transform(context,
			    (const uint32_t *)(const void *)context->buffer);
			context->bitcount += SHA256_BLOCK_LENGTH << 3;
			len -= SHA256_BLOCK_LENGTH;
			data += SHA256_BLOCK_LENGTH;
		}
	}
	if (len > 0) {
		/* There's left-overs, so save 'em */
		memcpy(context->buffer, data, len);
		context->bitcount += len << 3;
	}
	/* Clean up: */
	usedspace = freespace = 0;

	return 1;
}

static int
SHA224_256_Final(uint8_t digest[], SHA256_CTX *context, size_t len)
{
	unsigned int	usedspace;
	size_t i;

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != NULL) {
		usedspace = (unsigned int)((context->bitcount >> 3) %
		    SHA256_BLOCK_LENGTH);
		context->bitcount = htobe64(context->bitcount);
		if (usedspace > 0) {
			/* Begin padding with a 1 bit: */
			context->buffer[usedspace++] = 0x80;

			if (usedspace <= SHA256_SHORT_BLOCK_LENGTH) {
				/* Set-up for the last transform: */
				memset(&context->buffer[usedspace], 0,
				    (size_t)(SHA256_SHORT_BLOCK_LENGTH -
				    usedspace));
			} else {
				if (usedspace < SHA256_BLOCK_LENGTH) {
					memset(&context->buffer[usedspace], 0,
					    (size_t)(SHA256_BLOCK_LENGTH -
					    usedspace));
				}
				/* Do second-to-last transform: */
				SHA256_Transform(context,
				    (uint32_t *)(void *)context->buffer);

				/* And set-up for the last transform: */
				memset(context->buffer, 0,
				    (size_t)(SHA256_SHORT_BLOCK_LENGTH));
			}
		} else {
			/* Set-up for the last transform: */
			memset(context->buffer, 0,
			    (size_t)(SHA256_SHORT_BLOCK_LENGTH));

			/* Begin padding with a 1 bit: */
			*context->buffer = 0x80;
		}
		/* Set the bit count: */
		memcpy(&context->buffer[SHA256_SHORT_BLOCK_LENGTH],
		    &context->bitcount, sizeof(context->bitcount));

		/* Final transform: */
		SHA256_Transform(context, (uint32_t *)(void *)context->buffer);

		for (i = 0; i < len / 4; i++)
			be32enc(digest + 4 * i, context->state[i]);
	}

	/* Clean up state data: */
	memset(context, 0, sizeof(*context));
	usedspace = 0;

	return 1;
}

int
SHA256_Final(uint8_t digest[], SHA256_CTX *context)
{
	return SHA224_256_Final(digest, context, SHA256_DIGEST_LENGTH);
}

/*** SHA-224: *********************************************************/
int 
SHA224_Init(SHA224_CTX *context)
{
	if (context == NULL)
		return 1;

	/* The state and buffer size are driven by SHA256, not by SHA224. */
	memcpy(context->state, sha224_initial_hash_value,
	    (size_t)(SHA256_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA256_BLOCK_LENGTH));
	context->bitcount = 0;

	return 1;
}

int
SHA224_Update(SHA224_CTX *context, const uint8_t *data, size_t len)
{
	return SHA256_Update((SHA256_CTX *)context, data, len);
}

void
SHA224_Transform(SHA224_CTX *context, const uint32_t *data)
{
	SHA256_Transform((SHA256_CTX *)context, data);
}

int
SHA224_Final(uint8_t digest[], SHA224_CTX *context)
{
	return SHA224_256_Final(digest, (SHA256_CTX *)context,
	    SHA224_DIGEST_LENGTH);
}

/*** SHA-512: *********************************************************/
int
SHA512_Init(SHA512_CTX *context)
{
	if (context == NULL)
		return 1;

	memcpy(context->state, sha512_initial_hash_value,
	    (size_t)(SHA512_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA512_BLOCK_LENGTH));
	context->bitcount[0] = context->bitcount[1] =  0;

	return 1;
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-512 round macros: */
#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	W512[j] = be64toh(*data);		\
	++data;					\
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
             K512[j] + W512[j]; \
	(d) += T1, \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)), \
	j++

#define ROUND512(a,b,c,d,e,f,g,h)	\
	s0 = W512[(j+1)&0x0f]; \
	s0 = sigma0_512(s0); \
	s1 = W512[(j+14)&0x0f]; \
	s1 = sigma1_512(s1); \
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + K512[j] + \
             (W512[j&0x0f] += s1 + W512[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)); \
	j++

void
SHA512_Transform(SHA512_CTX *context, const uint64_t *data)
{
	uint64_t	a, b, c, d, e, f, g, h, s0, s1;
	uint64_t	T1, *W512 = (uint64_t *)context->buffer;
	int		j;

	/* Initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		ROUND512_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND512_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND512_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND512_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND512_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND512_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND512_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND512_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds up to 79: */
	do {
		ROUND512(a,b,c,d,e,f,g,h);
		ROUND512(h,a,b,c,d,e,f,g);
		ROUND512(g,h,a,b,c,d,e,f);
		ROUND512(f,g,h,a,b,c,d,e);
		ROUND512(e,f,g,h,a,b,c,d);
		ROUND512(d,e,f,g,h,a,b,c);
		ROUND512(c,d,e,f,g,h,a,b);
		ROUND512(b,c,d,e,f,g,h,a);
	} while (j < 80);

	/* Compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* SHA2_UNROLL_TRANSFORM */

void
SHA512_Transform(SHA512_CTX *context, const uint64_t *data)
{
	uint64_t	a, b, c, d, e, f, g, h, s0, s1;
	uint64_t	T1, T2, *W512 = (void *)context->buffer;
	int		j;

	/* Initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		W512[j] = be64toh(*data);
		++data;
		/* Apply the SHA-512 compression function to update a..h */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] + W512[j];
		T2 = Sigma0_512(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W512[(j+1)&0x0f];
		s0 = sigma0_512(s0);
		s1 = W512[(j+14)&0x0f];
		s1 =  sigma1_512(s1);

		/* Apply the SHA-512 compression function to update a..h */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] +
		     (W512[j&0x0f] += s1 + W512[(j+9)&0x0f] + s0);
		T2 = Sigma0_512(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 80);

	/* Compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

int
SHA512_Update(SHA512_CTX *context, const uint8_t *data, size_t len)
{
	unsigned int	freespace, usedspace;

	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return 1;
	}

	usedspace = (unsigned int)((context->bitcount[0] >> 3) %
	    SHA512_BLOCK_LENGTH);
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = SHA512_BLOCK_LENGTH - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			memcpy(&context->buffer[usedspace], data,
			    (size_t)(freespace));
			ADDINC128(context->bitcount, freespace << 3);
			len -= freespace;
			data += freespace;
			SHA512_Transform(context,
			    (uint64_t *)(void *)context->buffer);
		} else {
			/* The buffer is not yet full */
			memcpy(&context->buffer[usedspace], data, len);
			ADDINC128(context->bitcount, len << 3);
			/* Clean up: */
			usedspace = freespace = 0;
			return 1;
		}
	}
	/*
	 * Process as many complete blocks as possible.
	 *
	 * Check alignment of the data pointer. If it is 64bit aligned,
	 * SHA512_Transform can be called directly on the data stream,
	 * otherwise enforce the alignment by copy into the buffer.
	 */
	if ((uintptr_t)data % 8 == 0) {
		while (len >= SHA512_BLOCK_LENGTH) {
			SHA512_Transform(context,
			    (const uint64_t*)(const void *)data);
			ADDINC128(context->bitcount, SHA512_BLOCK_LENGTH << 3);
			len -= SHA512_BLOCK_LENGTH;
			data += SHA512_BLOCK_LENGTH;
		}
	} else {
		while (len >= SHA512_BLOCK_LENGTH) {
			memcpy(context->buffer, data, SHA512_BLOCK_LENGTH);
			SHA512_Transform(context,
			    (const void *)context->buffer);
			ADDINC128(context->bitcount, SHA512_BLOCK_LENGTH << 3);
			len -= SHA512_BLOCK_LENGTH;
			data += SHA512_BLOCK_LENGTH;
		}
	}
	if (len > 0) {
		/* There's left-overs, so save 'em */
		memcpy(context->buffer, data, len);
		ADDINC128(context->bitcount, len << 3);
	}
	/* Clean up: */
	usedspace = freespace = 0;

	return 1;
}

static void
SHA512_Last(SHA512_CTX *context)
{
	unsigned int	usedspace;

	usedspace = (unsigned int)((context->bitcount[0] >> 3) % SHA512_BLOCK_LENGTH);
	context->bitcount[0] = htobe64(context->bitcount[0]);
	context->bitcount[1] = htobe64(context->bitcount[1]);
	if (usedspace > 0) {
		/* Begin padding with a 1 bit: */
		context->buffer[usedspace++] = 0x80;

		if (usedspace <= SHA512_SHORT_BLOCK_LENGTH) {
			/* Set-up for the last transform: */
			memset(&context->buffer[usedspace], 0,
			    (size_t)(SHA512_SHORT_BLOCK_LENGTH - usedspace));
		} else {
			if (usedspace < SHA512_BLOCK_LENGTH) {
				memset(&context->buffer[usedspace], 0,
				    (size_t)(SHA512_BLOCK_LENGTH - usedspace));
			}
			/* Do second-to-last transform: */
			SHA512_Transform(context,
			    (uint64_t *)(void *)context->buffer);

			/* And set-up for the last transform: */
			memset(context->buffer, 0,
			    (size_t)(SHA512_BLOCK_LENGTH - 2));
		}
	} else {
		/* Prepare for final transform: */
		memset(context->buffer, 0, (size_t)(SHA512_SHORT_BLOCK_LENGTH));

		/* Begin padding with a 1 bit: */
		*context->buffer = 0x80;
	}
	/* Store the length of input data (in bits): */
	memcpy(&context->buffer[SHA512_SHORT_BLOCK_LENGTH],
	    &context->bitcount[1], sizeof(context->bitcount[1]));
	memcpy(&context->buffer[SHA512_SHORT_BLOCK_LENGTH + 8],
	    &context->bitcount[0], sizeof(context->bitcount[0]));

	/* Final transform: */
	SHA512_Transform(context, (uint64_t *)(void *)context->buffer);
}

int
SHA512_Final(uint8_t digest[], SHA512_CTX *context)
{
	size_t i;

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != NULL) {
		SHA512_Last(context);

		/* Save the hash data for output: */
		for (i = 0; i < 8; ++i)
			be64enc(digest + 8 * i, context->state[i]);
	}

	/* Zero out state data */
	memset(context, 0, sizeof(*context));

	return 1;
}

/*** SHA-384: *********************************************************/
int
SHA384_Init(SHA384_CTX *context)
{
	if (context == NULL)
		return 1;

	memcpy(context->state, sha384_initial_hash_value,
	    (size_t)(SHA512_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA384_BLOCK_LENGTH));
	context->bitcount[0] = context->bitcount[1] = 0;

	return 1;
}

int
SHA384_Update(SHA384_CTX *context, const uint8_t *data, size_t len)
{
	return SHA512_Update((SHA512_CTX *)context, data, len);
}

void
SHA384_Transform(SHA512_CTX *context, const uint64_t *data)
{
	SHA512_Transform((SHA512_CTX *)context, data);
}

int
SHA384_Final(uint8_t digest[], SHA384_CTX *context)
{
	size_t i;

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != NULL) {
		SHA512_Last((SHA512_CTX *)context);

		/* Save the hash data for output: */
		for (i = 0; i < 6; ++i)
			be64enc(digest + 8 * i, context->state[i]);
	}

	/* Zero out state data */
	memset(context, 0, sizeof(*context));

	return 1;
}
