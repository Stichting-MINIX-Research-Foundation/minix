/* $NetBSD: sha2.c,v 1.7 2007/07/18 14:09:55 joerg Exp $ */
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
#include <sys/types.h>
/* #include <sys/time.h> */
/* #include <sys/systm.h> */
/* #include <machine/endian.h> */
#include <stdint.h>
#include <minix/sha2.h>
#include <assert.h>

/*
 * ASSERT NOTE:
 * Some sanity checking code is included using assert().  On my FreeBSD
 * system, this additional code can be removed by compiling with NDEBUG
 * defined.  Check your own systems manpage on assert() to see how to
 * compile WITHOUT the sanity checking code on your system.
 *
 * UNROLLED TRANSFORM LOOP NOTE:
 * You can define SHA2_UNROLL_TRANSFORM to use the unrolled transform
 * loop version for the hash transform rounds (defined using macros
 * later in this file).  Either define on the command line, for example:
 *
 *   cc -DSHA2_UNROLL_TRANSFORM -o sha2 sha2.c sha2prog.c
 *
 * or define below:
 *
 *   #define SHA2_UNROLL_TRANSFORM
 *
 */

/*** SHA-256/384/512 Machine Architecture Definitions *****************/
/*
 * SHA2_BYTE_ORDER NOTE:
 *
 * Please make sure that your system defines SHA2_BYTE_ORDER.  If your
 * architecture is little-endian, make sure it also defines
 * SHA2_LITTLE_ENDIAN and that the two (SHA2_BYTE_ORDER and SHA2_LITTLE_ENDIAN) are
 * equivilent.
 *
 * If your system does not define the above, then you can do so by
 * hand like this:
 *
 *   #define SHA2_LITTLE_ENDIAN 1234
 *   #define SHA2_BIG_ENDIAN    4321
 *
 * And for little-endian machines, add:
 *
 *   #define SHA2_BYTE_ORDER SHA2_LITTLE_ENDIAN 
 *
 * Or for big-endian machines:
 *
 *   #define SHA2_BYTE_ORDER SHA2_BIG_ENDIAN
 *
 * The FreeBSD machine this was written on defines BYTE_ORDER
 * appropriately by including <sys/types.h> (which in turn includes
 * <machine/endian.h> where the appropriate definitions are actually
 * made).
 */
#if !defined(SHA2_BYTE_ORDER) || (SHA2_BYTE_ORDER != SHA2_LITTLE_ENDIAN && SHA2_BYTE_ORDER != SHA2_BIG_ENDIAN)
#error Define SHA2_BYTE_ORDER to be equal to either SHA2_LITTLE_ENDIAN or SHA2_BIG_ENDIAN
#endif

#if 0 /*def SHA2_USE_INTTYPES_H*/

typedef uint8_t  sha2_byte;	/* Exactly 1 byte */
typedef uint32_t sha2_word32;	/* Exactly 4 bytes */
typedef uint64_t sha2_word64;	/* Exactly 8 bytes */

#else /* SHA2_USE_INTTYPES_H */

typedef u_int8_t  sha2_byte;	/* Exactly 1 byte */
typedef u_int32_t sha2_word32;	/* Exactly 4 bytes */
typedef u_int64_t sha2_word64;	/* Exactly 8 bytes */

#endif /* SHA2_USE_INTTYPES_H */

/*** SHA-256/384/512 Various Length Definitions ***********************/
/* NOTE: Most of these are in sha2.h */
#define SHA256_SHORT_BLOCK_LENGTH	(SHA256_BLOCK_LENGTH - 8)
#define SHA384_SHORT_BLOCK_LENGTH	(SHA384_BLOCK_LENGTH - 16)
#define SHA512_SHORT_BLOCK_LENGTH	(SHA512_BLOCK_LENGTH - 16)

/*** ENDIAN REVERSAL MACROS *******************************************/
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
#define REVERSE32(w,x)	{ \
	sha2_word32 tmp = (w); \
	tmp = (tmp >> 16) | (tmp << 16); \
	(x) = (sha2_word32)(((tmp & 0xff00ff00UL) >> 8) | ((tmp & 0x00ff00ffUL) << 8)); \
}
#define REVERSE64(w,x)	{ \
	sha2_word64 tmp = (w); \
	tmp = (tmp >> 32) | (tmp << 32); \
	tmp = (sha2_word64)(((tmp & 0xff00ff00ff00ff00ULL) >> 8) | \
	      ((tmp & 0x00ff00ff00ff00ffULL) << 8)); \
	(x) = (sha2_word64)(((tmp & 0xffff0000ffff0000ULL) >> 16) | \
	      ((tmp & 0x0000ffff0000ffffULL) << 16)); \
}
#if MINIX_64BIT
#undef REVERSE64
#define REVERSE64(w,x)	{ \
	sha2_word64 tmp64 = (w); \
	u32_t hi, lo; \
	REVERSE32(ex64hi(tmp64), lo); \
	REVERSE32(ex64lo(tmp64), hi); \
	(x) = make64(lo, hi); \
}
#endif /* MINIX_64BIT */
#endif /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */

/*
 * Macro for incrementally adding the unsigned 64-bit integer n to the
 * unsigned 128-bit integer (represented using a two-element array of
 * 64-bit words):
 */
#define ADDINC128(w,n)	{ \
	(w)[0] = add64u((w)[0], (n)); \
	if (cmp64u((w)[0], (n)) < 0) { \
		(w)[1] = add64u((w)[1], 1); \
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
#define S64(b,x)	(rrotate64((x), (b)))
#define R64(b, x)   (rshift64(x, b))

/* Two of six logical functions used in SHA-256, SHA-384, and SHA-512: */
#define Ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define Ch64(x,y,z)	(xor64(and64((x), (y)), and64(not64((x)), (z))))
#define Maj64(x,y,z) (xor64(xor64(and64((x), (y)), and64((x), (z))), and64((y), (z))))

/* Four of six logical functions used in SHA-256: */
#define Sigma0_256(x)	(S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))
#define Sigma1_256(x)	(S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
#define sigma0_256(x)	(S32(7,  (x)) ^ S32(18, (x)) ^ R(3 ,   (x)))
#define sigma1_256(x)	(S32(17, (x)) ^ S32(19, (x)) ^ R(10,   (x)))

/* Four of six logical functions used in SHA-384 and SHA-512: */
#define Sigma0_512(x)	(xor64(xor64(S64(28, (x)), S64(34, (x))), S64(39, (x))))
#define Sigma1_512(x)	(xor64(xor64(S64(14, (x)), S64(18, (x))), S64(41, (x))))
#define sigma0_512(x)	(xor64(xor64(S64( 1, (x)), S64( 8, (x))), R64( 7, (x))))
#define sigma1_512(x)	(xor64(xor64(S64(19, (x)), S64(61, (x))), R64( 6, (x))))

/*** INTERNAL FUNCTION PROTOTYPES *************************************/
/* NOTE: These should not be accessed directly from outside this
 * library -- they are intended for private internal visibility/use
 * only.
 */
static void SHA512_Last(SHA512_CTX*);
void SHA256_Transform(SHA256_CTX*, const sha2_word32*);
void SHA384_Transform(SHA384_CTX*, const sha2_word64*);
void SHA512_Transform(SHA512_CTX*, const sha2_word64*);


/*** SHA-XYZ INITIAL HASH VALUES AND CONSTANTS ************************/
/* Hash constant words K for SHA-256: */
static const sha2_word32 K256[64] = {
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

/* Initial hash value H for SHA-256: */
static const sha2_word32 sha256_initial_hash_value[8] = {
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
const static sha2_word64 K512[80] = {
	{0xd728ae22UL, 0x428a2f98UL}, {0x23ef65cdUL, 0x71374491UL},
	{0xec4d3b2fUL, 0xb5c0fbcfUL}, {0x8189dbbcUL, 0xe9b5dba5UL},
	{0xf348b538UL, 0x3956c25bUL}, {0xb605d019UL, 0x59f111f1UL},
	{0xaf194f9bUL, 0x923f82a4UL}, {0xda6d8118UL, 0xab1c5ed5UL},
	{0xa3030242UL, 0xd807aa98UL}, {0x45706fbeUL, 0x12835b01UL},
	{0x4ee4b28cUL, 0x243185beUL}, {0xd5ffb4e2UL, 0x550c7dc3UL},
	{0xf27b896fUL, 0x72be5d74UL}, {0x3b1696b1UL, 0x80deb1feUL},
	{0x25c71235UL, 0x9bdc06a7UL}, {0xcf692694UL, 0xc19bf174UL},
	{0x9ef14ad2UL, 0xe49b69c1UL}, {0x384f25e3UL, 0xefbe4786UL},
	{0x8b8cd5b5UL, 0x0fc19dc6UL}, {0x77ac9c65UL, 0x240ca1ccUL},
	{0x592b0275UL, 0x2de92c6fUL}, {0x6ea6e483UL, 0x4a7484aaUL},
	{0xbd41fbd4UL, 0x5cb0a9dcUL}, {0x831153b5UL, 0x76f988daUL},
	{0xee66dfabUL, 0x983e5152UL}, {0x2db43210UL, 0xa831c66dUL},
	{0x98fb213fUL, 0xb00327c8UL}, {0xbeef0ee4UL, 0xbf597fc7UL},
	{0x3da88fc2UL, 0xc6e00bf3UL}, {0x930aa725UL, 0xd5a79147UL},
	{0xe003826fUL, 0x06ca6351UL}, {0x0a0e6e70UL, 0x14292967UL},
	{0x46d22ffcUL, 0x27b70a85UL}, {0x5c26c926UL, 0x2e1b2138UL},
	{0x5ac42aedUL, 0x4d2c6dfcUL}, {0x9d95b3dfUL, 0x53380d13UL},
	{0x8baf63deUL, 0x650a7354UL}, {0x3c77b2a8UL, 0x766a0abbUL},
	{0x47edaee6UL, 0x81c2c92eUL}, {0x1482353bUL, 0x92722c85UL},
	{0x4cf10364UL, 0xa2bfe8a1UL}, {0xbc423001UL, 0xa81a664bUL},
	{0xd0f89791UL, 0xc24b8b70UL}, {0x0654be30UL, 0xc76c51a3UL},
	{0xd6ef5218UL, 0xd192e819UL}, {0x5565a910UL, 0xd6990624UL},
	{0x5771202aUL, 0xf40e3585UL}, {0x32bbd1b8UL, 0x106aa070UL},
	{0xb8d2d0c8UL, 0x19a4c116UL}, {0x5141ab53UL, 0x1e376c08UL},
	{0xdf8eeb99UL, 0x2748774cUL}, {0xe19b48a8UL, 0x34b0bcb5UL},
	{0xc5c95a63UL, 0x391c0cb3UL}, {0xe3418acbUL, 0x4ed8aa4aUL},
	{0x7763e373UL, 0x5b9cca4fUL}, {0xd6b2b8a3UL, 0x682e6ff3UL},
	{0x5defb2fcUL, 0x748f82eeUL}, {0x43172f60UL, 0x78a5636fUL},
	{0xa1f0ab72UL, 0x84c87814UL}, {0x1a6439ecUL, 0x8cc70208UL},
	{0x23631e28UL, 0x90befffaUL}, {0xde82bde9UL, 0xa4506cebUL},
	{0xb2c67915UL, 0xbef9a3f7UL}, {0xe372532bUL, 0xc67178f2UL},
	{0xea26619cUL, 0xca273eceUL}, {0x21c0c207UL, 0xd186b8c7UL},
	{0xcde0eb1eUL, 0xeada7dd6UL}, {0xee6ed178UL, 0xf57d4f7fUL},
	{0x72176fbaUL, 0x06f067aaUL}, {0xa2c898a6UL, 0x0a637dc5UL},
	{0xbef90daeUL, 0x113f9804UL}, {0x131c471bUL, 0x1b710b35UL},
	{0x23047d84UL, 0x28db77f5UL}, {0x40c72493UL, 0x32caab7bUL},
	{0x15c9bebcUL, 0x3c9ebe0aUL}, {0x9c100d4cUL, 0x431d67c4UL},
	{0xcb3e42b6UL, 0x4cc5d4beUL}, {0xfc657e2aUL, 0x597f299cUL},
	{0x3ad6faecUL, 0x5fcb6fabUL}, {0x4a475817UL, 0x6c44198cUL}
};

/* Initial hash value H for SHA-384 */
const static sha2_word64 sha384_initial_hash_value[8] = {
	{0xc1059ed8UL, 0xcbbb9d5dUL},
	{0x367cd507UL, 0x629a292aUL},
	{0x3070dd17UL, 0x9159015aUL},
	{0xf70e5939UL, 0x152fecd8UL},
	{0xffc00b31UL, 0x67332667UL},
	{0x68581511UL, 0x8eb44a87UL},
	{0x64f98fa7UL, 0xdb0c2e0dUL},
	{0xbefa4fa4UL, 0x47b5481dUL}
};

const static sha2_word64 sha512_initial_hash_value[8] = {
	{0xf3bcc908UL, 0x6a09e667UL},
	{0x84caa73bUL, 0xbb67ae85UL},
	{0xfe94f82bUL, 0x3c6ef372UL},
	{0x5f1d36f1UL, 0xa54ff53aUL},
	{0xade682d1UL, 0x510e527fUL},
	{0x2b3e6c1fUL, 0x9b05688cUL},
	{0xfb41bd6bUL, 0x1f83d9abUL},
	{0x137e2179UL, 0x5be0cd19UL}
};

/*** SHA-256: *********************************************************/
void SHA256_Init(SHA256_CTX* context) {
	if (context == (SHA256_CTX*)0) {
		return;
	}
	memcpy(context->state, sha256_initial_hash_value, (size_t)(SHA256_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA256_BLOCK_LENGTH));
	context->bitcount = cvu64(0);
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-256 round macros: */

#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE32(*data++, W256[j]); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
             K256[j] + W256[j]; \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++


#else /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
	     K256[j] + (W256[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

#endif /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */

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

void SHA256_Transform(SHA256_CTX* context, const sha2_word32* data) {
	sha2_word32	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word32	T1, *W256;
	int		j;

	W256 = (sha2_word32*)context->buffer;

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

void SHA256_Transform(SHA256_CTX* context, const sha2_word32* data) {
	sha2_word32	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word32	T1, T2, *W256;
	int		j;

	W256 = (sha2_word32*)(void *)context->buffer;

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
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
		/* Copy data while converting to host byte order */
		REVERSE32(*data++,W256[j]);
		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + W256[j];
#else /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */
		/* Apply the SHA-256 compression function to update a..h with copy */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + (W256[j] = *data++);
#endif /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */
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

void SHA256_Update(SHA256_CTX* context, const sha2_byte *data, size_t len) {
	unsigned int	freespace, usedspace;

	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	assert(context != (SHA256_CTX*)0 && data != (sha2_byte*)0);

#if MINIX_64BIT
	usedspace= rem64u(context->bitcount, SHA256_BLOCK_LENGTH*8)/8;
#else /* !MINIX_64BIT */
	usedspace = (context->bitcount >> 3) % SHA256_BLOCK_LENGTH;
#endif /* MINIX_64BIT */
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = SHA256_BLOCK_LENGTH - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			memcpy(&context->buffer[usedspace], data, (size_t)(freespace));
#if MINIX_64BIT
			context->bitcount= add64u(context->bitcount,
				freespace << 3);
#else /* !MINIX_64BIT */
			context->bitcount += freespace << 3;
#endif /* MINIX_64BIT */
			len -= freespace;
			data += freespace;
			SHA256_Transform(context, (sha2_word32*)(void *)context->buffer);
		} else {
			/* The buffer is not yet full */
			memcpy(&context->buffer[usedspace], data, len);
#if MINIX_64BIT
			context->bitcount= add64u(context->bitcount, len << 3);
#else /* !MINIX_64BIT */
			context->bitcount += len << 3;
#endif /* MINIX_64BIT */
			/* Clean up: */
			usedspace = freespace = 0;
			return;
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
			    (const sha2_word32 *)(const void *)data);
#if MINIX_64BIT
			context->bitcount= add64u(context->bitcount,
				SHA256_BLOCK_LENGTH << 3);
#else /* !MINIX_64BIT */
			context->bitcount += SHA256_BLOCK_LENGTH << 3;
#endif /* !MINIX_64BIT */
			len -= SHA256_BLOCK_LENGTH;
			data += SHA256_BLOCK_LENGTH;
		}
	} else {
		while (len >= SHA256_BLOCK_LENGTH) {
			memcpy(context->buffer, data, SHA256_BLOCK_LENGTH);
			SHA256_Transform(context,
			    (const sha2_word32 *)(const void *)context->buffer);
#if MINIX_64BIT
			context->bitcount= add64u(context->bitcount,
				SHA256_BLOCK_LENGTH << 3);
#else /* !MINIX_64BIT */
			context->bitcount += SHA256_BLOCK_LENGTH << 3;
#endif /* MINIX_64BIT */
			len -= SHA256_BLOCK_LENGTH;
			data += SHA256_BLOCK_LENGTH;
		}
	}
	if (len > 0) {
		/* There's left-overs, so save 'em */
		memcpy(context->buffer, data, len);
#if MINIX_64BIT
		context->bitcount= add64u(context->bitcount, len << 3);
#else /* !MINIX_64BIT */
		context->bitcount += len << 3;
#endif /* MINIX_64BIT */
	}
	/* Clean up: */
	usedspace = freespace = 0;
}

void SHA256_Final(sha2_byte digest[], SHA256_CTX* context) {
	sha2_word32	*d = (void *)digest;
	unsigned int	usedspace;

	/* Sanity check: */
	assert(context != (SHA256_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (sha2_byte*)0) {
#if MINIX_64BIT
		usedspace= rem64u(context->bitcount, SHA256_BLOCK_LENGTH*8)/8;
#else /* !MINIX_64BIT */
		usedspace = (context->bitcount >> 3) % SHA256_BLOCK_LENGTH;
#endif /* MINIX_64BIT */

#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
		/* Convert FROM host byte order */
		REVERSE64(context->bitcount,context->bitcount);
#endif
		if (usedspace > 0) {
			/* Begin padding with a 1 bit: */
			context->buffer[usedspace++] = 0x80;

			if (usedspace <= SHA256_SHORT_BLOCK_LENGTH) {
				/* Set-up for the last transform: */
				memset(&context->buffer[usedspace], 0, (size_t)(SHA256_SHORT_BLOCK_LENGTH - usedspace));
			} else {
				if (usedspace < SHA256_BLOCK_LENGTH) {
					memset(&context->buffer[usedspace], 0, (size_t)(SHA256_BLOCK_LENGTH - usedspace));
				}
				/* Do second-to-last transform: */
				SHA256_Transform(context, (sha2_word32*)(void *)context->buffer);

				/* And set-up for the last transform: */
				memset(context->buffer, 0, (size_t)(SHA256_SHORT_BLOCK_LENGTH));
			}
		} else {
			/* Set-up for the last transform: */
			memset(context->buffer, 0, (size_t)(SHA256_SHORT_BLOCK_LENGTH));

			/* Begin padding with a 1 bit: */
			*context->buffer = 0x80;
		}
		/* Set the bit count: */
		*(sha2_word64*)(void *)&context->buffer[SHA256_SHORT_BLOCK_LENGTH] = context->bitcount;

		/* Final transform: */
		SHA256_Transform(context, (sha2_word32*)(void *)context->buffer);

#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < 8; j++) {
				REVERSE32(context->state[j],context->state[j]);
				*d++ = context->state[j];
			}
		}
#else
		memcpy(d, context->state, SHA256_DIGEST_LENGTH);
#endif
	}

	/* Clean up state data: */
	memset(context, 0, sizeof(*context));
	usedspace = 0;
}

/*** SHA-512: *********************************************************/
void SHA512_Init(SHA512_CTX* context) {
	if (context == (SHA512_CTX*)0) {
		return;
	}
	memcpy(context->state, sha512_initial_hash_value, (size_t)(SHA512_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA512_BLOCK_LENGTH));
	make_zero64(context->bitcount[0]);
	make_zero64(context->bitcount[1]);
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-512 round macros: */
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN

#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE64(*data++, W512[j]); \
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
             K512[j] + W512[j]; \
	(d) += T1, \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)), \
	j++


#else /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */

#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
             K512[j] + (W512[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)); \
	j++

#endif /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */

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

void SHA512_Transform(SHA512_CTX* context, const sha2_word64* data) {
	sha2_word64	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word64	T1, *W512 = (sha2_word64*)context->buffer;
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

void SHA512_Transform(SHA512_CTX* context, const sha2_word64* data) {
	sha2_word64	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word64	T1, T2, *W512 = (void *)context->buffer;
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
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
		/* Convert TO host byte order */
		REVERSE64(*data++, W512[j]);
#else /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */
		W512[j] = *data++;
#endif /* SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN */
		/* Apply the SHA-512 compression function to update a..h */
		T1 = add64(add64(add64(add64(h, Sigma1_512(e)), Ch64(e, f, g)), K512[j]), W512[j]);
		T2 = add64(Sigma0_512(a), Maj64(a, b, c));

		h = g;
		g = f;
		f = e;
		e = add64(d, T1);
		d = c;
		c = b;
		b = a;
		a = add64(T1, T2);

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W512[(j+1)&0x0f];
		s0 = sigma0_512(s0);
		s1 = W512[(j+14)&0x0f];
		s1 =  sigma1_512(s1);

		/* Apply the SHA-512 compression function to update a..h */
		W512[j&0x0f] = add64(add64(add64(W512[j&0x0f], s1), W512[(j+9)&0x0f]),  s0);
		T1 = add64(add64(add64(add64(h, Sigma1_512(e)), Ch64(e, f, g)), K512[j]), W512[j&0x0f]);
		T2 = add64(Sigma0_512(a), Maj64(a, b, c));
		h = g;
		g = f;
		f = e;
		e = add64(d, T1);
		d = c;
		c = b;
		b = a;
		a = add64(T1, T2);

		j++;
	} while (j < 80);

	/* Compute the current intermediate hash value */
	context->state[0] = add64(context->state[0], a);
	context->state[1] = add64(context->state[1], b);
	context->state[2] = add64(context->state[2], c);
	context->state[3] = add64(context->state[3], d);
	context->state[4] = add64(context->state[4], e);
	context->state[5] = add64(context->state[5], f);
	context->state[6] = add64(context->state[6], g);
	context->state[7] = add64(context->state[7], h);

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = T2 = cvu64(0);
}

#endif /* SHA2_UNROLL_TRANSFORM */

void SHA512_Update(SHA512_CTX* context, const sha2_byte *data, size_t len) {
	unsigned int	freespace, usedspace;

	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	assert(context != (SHA512_CTX*)0 && data != (sha2_byte*)0);

	usedspace = (unsigned int)rem64u(rshift64(context->bitcount[0], 3), SHA512_BLOCK_LENGTH);
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = SHA512_BLOCK_LENGTH - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			memcpy(&context->buffer[usedspace], data, (size_t)(freespace));
			ADDINC128(context->bitcount, freespace << 3);
			len -= freespace;
			data += freespace;
			SHA512_Transform(context, (sha2_word64*)(void *)context->buffer);
		} else {
			/* The buffer is not yet full */
			memcpy(&context->buffer[usedspace], data, len);
			ADDINC128(context->bitcount, len << 3);
			/* Clean up: */
			usedspace = freespace = 0;
			return;
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
			    (const sha2_word64 *)(const void *)data);
			ADDINC128(context->bitcount, SHA512_BLOCK_LENGTH << 3);
			len -= SHA512_BLOCK_LENGTH;
			data += SHA512_BLOCK_LENGTH;
		}
	} else {
		while (len >= SHA512_BLOCK_LENGTH) {
			memcpy(context->buffer, data, SHA512_BLOCK_LENGTH);
			SHA512_Transform(context,
			    (const sha2_word64 *)(void *)context->buffer);
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
}

static void SHA512_Last(SHA512_CTX* context) {
	unsigned int	usedspace;

	usedspace = rem64u(rshift64(context->bitcount[0], 3), SHA512_BLOCK_LENGTH);
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
	/* Convert FROM host byte order */
	REVERSE64(context->bitcount[0],context->bitcount[0]);
	REVERSE64(context->bitcount[1],context->bitcount[1]);
#endif
	if (usedspace > 0) {
		/* Begin padding with a 1 bit: */
		context->buffer[usedspace++] = 0x80;

		if (usedspace <= SHA512_SHORT_BLOCK_LENGTH) {
			/* Set-up for the last transform: */
			memset(&context->buffer[usedspace], 0, (size_t)(SHA512_SHORT_BLOCK_LENGTH - usedspace));
		} else {
			if (usedspace < SHA512_BLOCK_LENGTH) {
				memset(&context->buffer[usedspace], 0, (size_t)(SHA512_BLOCK_LENGTH - usedspace));
			}
			/* Do second-to-last transform: */
			SHA512_Transform(context, (sha2_word64*)(void *)context->buffer);

			/* And set-up for the last transform: */
			memset(context->buffer, 0, (size_t)(SHA512_BLOCK_LENGTH - 2));
		}
	} else {
		/* Prepare for final transform: */
		memset(context->buffer, 0, (size_t)(SHA512_SHORT_BLOCK_LENGTH));

		/* Begin padding with a 1 bit: */
		*context->buffer = 0x80;
	}
	/* Store the length of input data (in bits): */
	*(sha2_word64*)(void *)&context->buffer[SHA512_SHORT_BLOCK_LENGTH] = context->bitcount[1];
	*(sha2_word64*)(void *)&context->buffer[SHA512_SHORT_BLOCK_LENGTH+8] = context->bitcount[0];

	/* Final transform: */
	SHA512_Transform(context, (sha2_word64*)(void *)context->buffer);
}

void SHA512_Final(sha2_byte digest[], SHA512_CTX* context) {
	sha2_word64	*d = (void *)digest;

	/* Sanity check: */
	assert(context != (SHA512_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (sha2_byte*)0) {
		SHA512_Last(context);

		/* Save the hash data for output: */
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < 8; j++) {
				REVERSE64(context->state[j],context->state[j]);
				*d++ = context->state[j];
			}
		}
#else
		memcpy(d, context->state, SHA512_DIGEST_LENGTH);
#endif
	}

	/* Zero out state data */
	memset(context, 0, sizeof(*context));
}

/*** SHA-384: *********************************************************/
void SHA384_Init(SHA384_CTX* context) {
	if (context == (SHA384_CTX*)0) {
		return;
	}
	memcpy(context->state, sha384_initial_hash_value, (size_t)(SHA512_DIGEST_LENGTH));
	memset(context->buffer, 0, (size_t)(SHA384_BLOCK_LENGTH));
	make_zero64(context->bitcount[0]);
	make_zero64(context->bitcount[1]);
}

void SHA384_Update(SHA384_CTX* context, const sha2_byte* data, size_t len) {
	SHA512_Update((SHA512_CTX*)context, data, len);
}

void SHA384_Transform(SHA512_CTX* context, const sha2_word64* data) {
	SHA512_Transform((SHA512_CTX*)context, data);
}

void SHA384_Final(sha2_byte digest[], SHA384_CTX* context) {
	sha2_word64	*d = (void *)digest;

	/* Sanity check: */
	assert(context != (SHA384_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (sha2_byte*)0) {
		SHA512_Last((SHA512_CTX*)context);

		/* Save the hash data for output: */
#if SHA2_BYTE_ORDER == SHA2_LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < 6; j++) {
				REVERSE64(context->state[j],context->state[j]);
				*d++ = context->state[j];
			}
		}
#else
		memcpy(d, context->state, SHA384_DIGEST_LENGTH);
#endif
	}

	/* Zero out state data */
	memset(context, 0, sizeof(*context));
}
