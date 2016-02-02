/*	$NetBSD: arc4random.c,v 1.30 2015/05/13 23:15:57 justin Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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

/*
 * Legacy arc4random(3) API from OpenBSD reimplemented using the
 * ChaCha20 PRF, with per-thread state.
 *
 * Security model:
 * - An attacker who sees some outputs cannot predict past or future
 *   outputs.
 * - An attacker who sees the PRNG state cannot predict past outputs.
 * - An attacker who sees a child's PRNG state cannot predict past or
 *   future outputs in the parent, or in other children.
 *
 * The arc4random(3) API may abort the process if:
 *
 * (a) the crypto self-test fails,
 * (b) pthread_atfork or thr_keycreate fail, or
 * (c) sysctl(KERN_ARND) fails when reseeding the PRNG.
 *
 * The crypto self-test, pthread_atfork, and thr_keycreate occur only
 * once, on the first use of any of the arc4random(3) API.  KERN_ARND
 * is unlikely to fail later unless the kernel is seriously broken.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: arc4random.c,v 1.30 2015/05/13 23:15:57 justin Exp $");

#include "namespace.h"
#include "reentrant.h"

#include <sys/bitops.h>
#include <sys/endian.h>
#include <sys/errno.h>
#if defined(__minix)
#include <sys/fcntl.h>
#endif
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <assert.h>
#include <sha2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(arc4random,_arc4random)
__weak_alias(arc4random_addrandom,_arc4random_addrandom)
__weak_alias(arc4random_buf,_arc4random_buf)
__weak_alias(arc4random_stir,_arc4random_stir)
__weak_alias(arc4random_uniform,_arc4random_uniform)
#endif

/*
 * For standard ChaCha, use le32dec/le32enc.  We don't need that for
 * the purposes of a nondeterministic random number generator -- we
 * don't need to be bit-for-bit compatible over any wire.
 */

static inline uint32_t
crypto_le32dec(const void *p)
{
	uint32_t v;

	(void)memcpy(&v, p, sizeof v);

	return v;
}

static inline void
crypto_le32enc(void *p, uint32_t v)
{

	(void)memcpy(p, &v, sizeof v);
}

/* ChaCha core */

#define	crypto_core_OUTPUTBYTES	64
#define	crypto_core_INPUTBYTES	16
#define	crypto_core_KEYBYTES	32
#define	crypto_core_CONSTBYTES	16

#define	crypto_core_ROUNDS	20

static uint32_t
rotate(uint32_t u, unsigned c)
{

	return (u << c) | (u >> (32 - c));
}

#define	QUARTERROUND(a, b, c, d) do {					      \
	(a) += (b); (d) ^= (a); (d) = rotate((d), 16);			      \
	(c) += (d); (b) ^= (c); (b) = rotate((b), 12);			      \
	(a) += (b); (d) ^= (a); (d) = rotate((d),  8);			      \
	(c) += (d); (b) ^= (c); (b) = rotate((b),  7);			      \
} while (/*CONSTCOND*/0)

const uint8_t crypto_core_constant32[16] = "expand 32-byte k";

static void
crypto_core(uint8_t *out, const uint8_t *in, const uint8_t *k,
    const uint8_t *c)
{
	uint32_t x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15;
	uint32_t j0,j1,j2,j3,j4,j5,j6,j7,j8,j9,j10,j11,j12,j13,j14,j15;
	int i;

	j0 = x0 = crypto_le32dec(c + 0);
	j1 = x1 = crypto_le32dec(c + 4);
	j2 = x2 = crypto_le32dec(c + 8);
	j3 = x3 = crypto_le32dec(c + 12);
	j4 = x4 = crypto_le32dec(k + 0);
	j5 = x5 = crypto_le32dec(k + 4);
	j6 = x6 = crypto_le32dec(k + 8);
	j7 = x7 = crypto_le32dec(k + 12);
	j8 = x8 = crypto_le32dec(k + 16);
	j9 = x9 = crypto_le32dec(k + 20);
	j10 = x10 = crypto_le32dec(k + 24);
	j11 = x11 = crypto_le32dec(k + 28);
	j12 = x12 = crypto_le32dec(in + 0);
	j13 = x13 = crypto_le32dec(in + 4);
	j14 = x14 = crypto_le32dec(in + 8);
	j15 = x15 = crypto_le32dec(in + 12);

	for (i = crypto_core_ROUNDS; i > 0; i -= 2) {
		QUARTERROUND( x0, x4, x8,x12);
		QUARTERROUND( x1, x5, x9,x13);
		QUARTERROUND( x2, x6,x10,x14);
		QUARTERROUND( x3, x7,x11,x15);
		QUARTERROUND( x0, x5,x10,x15);
		QUARTERROUND( x1, x6,x11,x12);
		QUARTERROUND( x2, x7, x8,x13);
		QUARTERROUND( x3, x4, x9,x14);
	}

	crypto_le32enc(out + 0, x0 + j0);
	crypto_le32enc(out + 4, x1 + j1);
	crypto_le32enc(out + 8, x2 + j2);
	crypto_le32enc(out + 12, x3 + j3);
	crypto_le32enc(out + 16, x4 + j4);
	crypto_le32enc(out + 20, x5 + j5);
	crypto_le32enc(out + 24, x6 + j6);
	crypto_le32enc(out + 28, x7 + j7);
	crypto_le32enc(out + 32, x8 + j8);
	crypto_le32enc(out + 36, x9 + j9);
	crypto_le32enc(out + 40, x10 + j10);
	crypto_le32enc(out + 44, x11 + j11);
	crypto_le32enc(out + 48, x12 + j12);
	crypto_le32enc(out + 52, x13 + j13);
	crypto_le32enc(out + 56, x14 + j14);
	crypto_le32enc(out + 60, x15 + j15);
}

/* ChaCha self-test */

#ifdef _DIAGNOSTIC

/*
 * Test vector for ChaCha20 from
 * <http://tools.ietf.org/html/draft-strombergson-chacha-test-vectors-00>,
 * test vectors for ChaCha12 and ChaCha8 and for big-endian machines
 * generated by the same crypto_core code with crypto_core_ROUNDS and
 * crypto_le32enc/dec varied.
 */

static const uint8_t crypto_core_selftest_vector[64] = {
#if _BYTE_ORDER == _LITTLE_ENDIAN
#  if crypto_core_ROUNDS == 8
	0x3e,0x00,0xef,0x2f,0x89,0x5f,0x40,0xd6,
	0x7f,0x5b,0xb8,0xe8,0x1f,0x09,0xa5,0xa1,
	0x2c,0x84,0x0e,0xc3,0xce,0x9a,0x7f,0x3b,
	0x18,0x1b,0xe1,0x88,0xef,0x71,0x1a,0x1e,
	0x98,0x4c,0xe1,0x72,0xb9,0x21,0x6f,0x41,
	0x9f,0x44,0x53,0x67,0x45,0x6d,0x56,0x19,
	0x31,0x4a,0x42,0xa3,0xda,0x86,0xb0,0x01,
	0x38,0x7b,0xfd,0xb8,0x0e,0x0c,0xfe,0x42,
#  elif crypto_core_ROUNDS == 12
	0x9b,0xf4,0x9a,0x6a,0x07,0x55,0xf9,0x53,
	0x81,0x1f,0xce,0x12,0x5f,0x26,0x83,0xd5,
	0x04,0x29,0xc3,0xbb,0x49,0xe0,0x74,0x14,
	0x7e,0x00,0x89,0xa5,0x2e,0xae,0x15,0x5f,
	0x05,0x64,0xf8,0x79,0xd2,0x7a,0xe3,0xc0,
	0x2c,0xe8,0x28,0x34,0xac,0xfa,0x8c,0x79,
	0x3a,0x62,0x9f,0x2c,0xa0,0xde,0x69,0x19,
	0x61,0x0b,0xe8,0x2f,0x41,0x13,0x26,0xbe,
#  elif crypto_core_ROUNDS == 20
	0x76,0xb8,0xe0,0xad,0xa0,0xf1,0x3d,0x90,
	0x40,0x5d,0x6a,0xe5,0x53,0x86,0xbd,0x28,
	0xbd,0xd2,0x19,0xb8,0xa0,0x8d,0xed,0x1a,
	0xa8,0x36,0xef,0xcc,0x8b,0x77,0x0d,0xc7,
	0xda,0x41,0x59,0x7c,0x51,0x57,0x48,0x8d,
	0x77,0x24,0xe0,0x3f,0xb8,0xd8,0x4a,0x37,
	0x6a,0x43,0xb8,0xf4,0x15,0x18,0xa1,0x1c,
	0xc3,0x87,0xb6,0x69,0xb2,0xee,0x65,0x86,
#  else
#    error crypto_core_ROUNDS must be 8, 12, or 20.
#  endif
#elif _BYTE_ORDER == _BIG_ENDIAN
#  if crypto_core_ROUNDS == 8
	0x9a,0x13,0x07,0xe3,0x38,0x18,0x9e,0x99,
	0x15,0x37,0x16,0x4d,0x04,0xe6,0x48,0x9a,
	0x07,0xd6,0xe8,0x7a,0x02,0xf9,0xf5,0xc7,
	0x3f,0xa9,0xc2,0x0a,0xe1,0xc6,0x62,0xea,
	0x80,0xaf,0xb6,0x51,0xca,0x52,0x43,0x87,
	0xe3,0xa6,0xa6,0x61,0x11,0xf5,0xe6,0xcf,
	0x09,0x0f,0xdc,0x9d,0xc3,0xc3,0xbb,0x43,
	0xd7,0xfa,0x70,0x42,0xbf,0xa5,0xee,0xa2,
#  elif crypto_core_ROUNDS == 12
	0xcf,0x6c,0x16,0x48,0xbf,0xf4,0xba,0x85,
	0x32,0x69,0xd3,0x98,0xc8,0x7d,0xcd,0x3f,
	0xdc,0x76,0x6b,0xa2,0x7b,0xcb,0x17,0x4d,
	0x05,0xda,0xdd,0xd8,0x62,0x54,0xbf,0xe0,
	0x65,0xed,0x0e,0xf4,0x01,0x7e,0x3c,0x05,
	0x35,0xb2,0x7a,0x60,0xf3,0x8f,0x12,0x33,
	0x24,0x60,0xcd,0x85,0xfe,0x4c,0xf3,0x39,
	0xb1,0x0e,0x3e,0xe0,0xba,0xa6,0x2f,0xa9,
#  elif crypto_core_ROUNDS == 20
	0x83,0x8b,0xf8,0x75,0xf7,0xde,0x9d,0x8c,
	0x33,0x14,0x72,0x28,0xd1,0xbe,0x88,0xe5,
	0x94,0xb5,0xed,0xb8,0x56,0xb5,0x9e,0x0c,
	0x64,0x6a,0xaf,0xd9,0xa7,0x49,0x10,0x59,
	0xba,0x3a,0x82,0xf8,0x4a,0x70,0x9c,0x00,
	0x82,0x2c,0xae,0xc6,0xd7,0x1c,0x2e,0xda,
	0x2a,0xfb,0x61,0x70,0x2b,0xd1,0xbf,0x8b,
	0x95,0xbc,0x23,0xb6,0x4b,0x60,0x02,0xec,
#  else
#    error crypto_core_ROUNDS must be 8, 12, or 20.
#  endif
#else
#  error Byte order must be little-endian or big-endian.
#endif
};

static int
crypto_core_selftest(void)
{
	const uint8_t nonce[crypto_core_INPUTBYTES] = {0};
	const uint8_t key[crypto_core_KEYBYTES] = {0};
	uint8_t block[64];
	unsigned i;

	crypto_core(block, nonce, key, crypto_core_constant32);
	for (i = 0; i < 64; i++) {
		if (block[i] != crypto_core_selftest_vector[i])
			return EIO;
	}

	return 0;
}

#else  /* !_DIAGNOSTIC */

static int
crypto_core_selftest(void)
{

	return 0;
}

#endif

/* PRNG */

/*
 * For a state s, rather than use ChaCha20 as a stream cipher to
 * generate the concatenation ChaCha20_s(0) || ChaCha20_s(1) || ..., we
 * split ChaCha20_s(0) into s' || x and yield x for the first request,
 * split ChaCha20_s'(0) into s'' || y and yield y for the second
 * request, &c.  This provides backtracking resistance: an attacker who
 * finds s'' can't recover s' or x.
 */

#define	crypto_prng_SEEDBYTES		crypto_core_KEYBYTES
#define	crypto_prng_MAXOUTPUTBYTES	\
	(crypto_core_OUTPUTBYTES - crypto_prng_SEEDBYTES)

struct crypto_prng {
	uint8_t		state[crypto_prng_SEEDBYTES];
};

static void
crypto_prng_seed(struct crypto_prng *prng, const void *seed)
{

	(void)memcpy(prng->state, seed, crypto_prng_SEEDBYTES);
}

static void
crypto_prng_buf(struct crypto_prng *prng, void *buf, size_t n)
{
	const uint8_t nonce[crypto_core_INPUTBYTES] = {0};
	uint8_t output[crypto_core_OUTPUTBYTES];

	_DIAGASSERT(n <= crypto_prng_MAXOUTPUTBYTES);
	__CTASSERT(sizeof prng->state + crypto_prng_MAXOUTPUTBYTES
	    <= sizeof output);

	crypto_core(output, nonce, prng->state, crypto_core_constant32);
	(void)memcpy(prng->state, output, sizeof prng->state);
	(void)memcpy(buf, output + sizeof prng->state, n);
	(void)explicit_memset(output, 0, sizeof output);
}

/* One-time stream: expand short single-use secret into long secret */

#define	crypto_onetimestream_SEEDBYTES	crypto_core_KEYBYTES

static void
crypto_onetimestream(const void *seed, void *buf, size_t n)
{
	uint32_t nonce[crypto_core_INPUTBYTES / sizeof(uint32_t)] = {0};
	uint8_t block[crypto_core_OUTPUTBYTES];
	uint8_t *p8, *p32;
	const uint8_t *nonce8 = (const uint8_t *)(void *)nonce;
	size_t ni, nb, nf;

	/*
	 * Guarantee we can generate up to n bytes.  We have
	 * 2^(8*INPUTBYTES) possible inputs yielding output of
	 * OUTPUTBYTES*2^(8*INPUTBYTES) bytes.  It suffices to require
	 * that sizeof n > (1/CHAR_BIT) log_2 n be less than
	 * (1/CHAR_BIT) log_2 of the total output stream length.  We
	 * have
	 *
	 *	log_2 (o 2^(8 i)) = log_2 o + log_2 2^(8 i)
	 *	  = log_2 o + 8 i.
	 */
	__CTASSERT(CHAR_BIT * sizeof n <=
	    (/*LINTED*/ilog2(crypto_core_OUTPUTBYTES) + 8 * crypto_core_INPUTBYTES));

	p8 = buf;
	p32 = (uint8_t *)roundup2((uintptr_t)p8, 4);
	ni = p32 - p8;
	if (n < ni)
		ni = n;
	nb = (n - ni) / sizeof block;
	nf = (n - ni) % sizeof block;

	_DIAGASSERT(((uintptr_t)p32 & 3) == 0);
	_DIAGASSERT(ni <= n);
	_DIAGASSERT(nb <= (n / sizeof block));
	_DIAGASSERT(nf <= n);
	_DIAGASSERT(n == (ni + (nb * sizeof block) + nf));
	_DIAGASSERT(ni < 4);
	_DIAGASSERT(nf < sizeof block);

	if (ni) {
		crypto_core(block, nonce8, seed, crypto_core_constant32);
		nonce[0]++;
		(void)memcpy(p8, block, ni);
	}
	while (nb--) {
		crypto_core(p32, nonce8, seed, crypto_core_constant32);
		if (++nonce[0] == 0)
			nonce[1]++;
		p32 += crypto_core_OUTPUTBYTES;
	}
	if (nf) {
		crypto_core(block, nonce8, seed, crypto_core_constant32);
		if (++nonce[0] == 0)
			nonce[1]++;
		(void)memcpy(p32, block, nf);
	}

	if (ni | nf)
		(void)explicit_memset(block, 0, sizeof block);
}

/* arc4random state: per-thread, per-process (zeroed in child on fork) */

struct arc4random_prng {
	struct crypto_prng	arc4_prng;
	bool			arc4_seeded;
};

static void
arc4random_prng_addrandom(struct arc4random_prng *prng, const void *data,
    size_t datalen)
{
#if !defined(__minix)
	const int mib[] = { CTL_KERN, KERN_ARND };
#endif /* !defined(__minix) */
	SHA256_CTX ctx;
	uint8_t buf[crypto_prng_SEEDBYTES];
	size_t buflen = sizeof buf;

	__CTASSERT(sizeof buf == SHA256_DIGEST_LENGTH);

	SHA256_Init(&ctx);

	crypto_prng_buf(&prng->arc4_prng, buf, sizeof buf);
	SHA256_Update(&ctx, buf, sizeof buf);

#if defined(__minix)
	/* LSC: We do not have a compatibility layer for the 
	 * KERN_ARND call, so do it the old way... */
	int fd;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd != -1) {
		(void)read(fd, buf, buflen);
		close(fd);
	}

        /* fd < 0 or failed sysctl ?  Ah, what the heck. We'll just take
         * whatever was on the stack... */
#else
	if (sysctl(mib, (u_int)__arraycount(mib), buf, &buflen, NULL, 0) == -1)
		abort();
#endif /* !defined(__minix) */
	if (buflen != sizeof buf)
		abort();
	SHA256_Update(&ctx, buf, sizeof buf);

	if (data != NULL)
		SHA256_Update(&ctx, data, datalen);

	SHA256_Final(buf, &ctx);
	(void)explicit_memset(&ctx, 0, sizeof ctx);

	/* reseed(SHA256(prng() || sysctl(KERN_ARND) || data)) */
	crypto_prng_seed(&prng->arc4_prng, buf);
	(void)explicit_memset(buf, 0, sizeof buf);
	prng->arc4_seeded = true;
}

#ifdef _REENTRANT
static struct arc4random_prng *
arc4random_prng_create(void)
{
	struct arc4random_prng *prng;
	const size_t size = roundup(sizeof(*prng), sysconf(_SC_PAGESIZE));

	prng = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
	if (prng == MAP_FAILED)
		goto fail0;
	if (minherit(prng, size, MAP_INHERIT_ZERO) == -1)
		goto fail1;

	return prng;

fail1:	(void)munmap(prng, size);
fail0:	return NULL;
}
#endif

#ifdef _REENTRANT
static void
arc4random_prng_destroy(struct arc4random_prng *prng)
{
	const size_t size = roundup(sizeof(*prng), sysconf(_SC_PAGESIZE));

	(void)explicit_memset(prng, 0, sizeof(*prng));
	(void)munmap(prng, size);
}
#endif

/* Library state */

static struct arc4random_global {
#ifdef _REENTRANT
	mutex_t			lock;
	thread_key_t		thread_key;
#endif
	struct arc4random_prng	prng;
	bool			initialized;
} arc4random_global = {
#ifdef _REENTRANT
	.lock		= MUTEX_INITIALIZER,
#endif
	.initialized	= false,
};

static void
arc4random_atfork_prepare(void)
{

	mutex_lock(&arc4random_global.lock);
	(void)explicit_memset(&arc4random_global.prng, 0,
	    sizeof arc4random_global.prng);
}

static void
arc4random_atfork_parent(void)
{

	mutex_unlock(&arc4random_global.lock);
}

static void
arc4random_atfork_child(void)
{

	mutex_unlock(&arc4random_global.lock);
}

#ifdef _REENTRANT
static void
arc4random_tsd_destructor(void *p)
{
	struct arc4random_prng *const prng = p;

	arc4random_prng_destroy(prng);
}
#endif

static void
arc4random_initialize(void)
{

	mutex_lock(&arc4random_global.lock);
	if (!arc4random_global.initialized) {
		if (crypto_core_selftest() != 0)
			abort();
#if !defined(__minix)
		if (pthread_atfork(&arc4random_atfork_prepare,
			&arc4random_atfork_parent, &arc4random_atfork_child)
		    != 0)
			abort();
#endif /* !defined(__minix) */
#ifdef _REENTRANT
		if (thr_keycreate(&arc4random_global.thread_key,
			&arc4random_tsd_destructor) != 0)
			abort();
#endif
		arc4random_global.initialized = true;
	}
	mutex_unlock(&arc4random_global.lock);
}

static struct arc4random_prng *
arc4random_prng_get(void)
{
	struct arc4random_prng *prng = NULL;

	/* Make sure the library is initialized.  */
	if (__predict_false(!arc4random_global.initialized))
		arc4random_initialize();

#ifdef _REENTRANT
	/* Get or create the per-thread PRNG state.  */
	prng = thr_getspecific(arc4random_global.thread_key);
	if (__predict_false(prng == NULL)) {
		prng = arc4random_prng_create();
		thr_setspecific(arc4random_global.thread_key, prng);
	}
#endif

	/* If we can't create it, fall back to the global PRNG.  */
	if (__predict_false(prng == NULL)) {
		mutex_lock(&arc4random_global.lock);
		prng = &arc4random_global.prng;
	}

	/* Guarantee the PRNG is seeded.  */
	if (__predict_false(!prng->arc4_seeded))
		arc4random_prng_addrandom(prng, NULL, 0);

	return prng;
}

static void
arc4random_prng_put(struct arc4random_prng *prng)
{

	/* If we had fallen back to the global PRNG, unlock it.  */
	if (__predict_false(prng == &arc4random_global.prng))
		mutex_unlock(&arc4random_global.lock);
}

/* Public API */

uint32_t
arc4random(void)
{
	struct arc4random_prng *prng;
	uint32_t v;

	prng = arc4random_prng_get();
	crypto_prng_buf(&prng->arc4_prng, &v, sizeof v);
	arc4random_prng_put(prng);

	return v;
}

void
arc4random_buf(void *buf, size_t len)
{
	struct arc4random_prng *prng;

	if (len <= crypto_prng_MAXOUTPUTBYTES) {
		prng = arc4random_prng_get();
		crypto_prng_buf(&prng->arc4_prng, buf, len);
		arc4random_prng_put(prng);
	} else {
		uint8_t seed[crypto_onetimestream_SEEDBYTES];

		prng = arc4random_prng_get();
		crypto_prng_buf(&prng->arc4_prng, seed, sizeof seed);
		arc4random_prng_put(prng);

		crypto_onetimestream(seed, buf, len);
		(void)explicit_memset(seed, 0, sizeof seed);
	}
}

uint32_t
arc4random_uniform(uint32_t bound)
{
	struct arc4random_prng *prng;
	uint32_t minimum, r;

	/*
	 * We want a uniform random choice in [0, n), and arc4random()
	 * makes a uniform random choice in [0, 2^32).  If we reduce
	 * that modulo n, values in [0, 2^32 mod n) will be represented
	 * slightly more than values in [2^32 mod n, n).  Instead we
	 * choose only from [2^32 mod n, 2^32) by rejecting samples in
	 * [0, 2^32 mod n), to avoid counting the extra representative
	 * of [0, 2^32 mod n).  To compute 2^32 mod n, note that
	 *
	 *	2^32 mod n = 2^32 mod n - 0
	 *	  = 2^32 mod n - n mod n
	 *	  = (2^32 - n) mod n,
	 *
	 * the last of which is what we compute in 32-bit arithmetic.
	 */
	minimum = (-bound % bound);

	prng = arc4random_prng_get();
	do crypto_prng_buf(&prng->arc4_prng, &r, sizeof r);
	while (__predict_false(r < minimum));
	arc4random_prng_put(prng);

	return (r % bound);
}

void
arc4random_stir(void)
{
	struct arc4random_prng *prng;

	prng = arc4random_prng_get();
	arc4random_prng_addrandom(prng, NULL, 0);
	arc4random_prng_put(prng);
}

/*
 * Silly signature here is for hysterical raisins.  Should instead be
 * const void *data and size_t datalen.
 */
void
arc4random_addrandom(u_char *data, int datalen)
{
	struct arc4random_prng *prng;

	_DIAGASSERT(0 <= datalen);

	prng = arc4random_prng_get();
	arc4random_prng_addrandom(prng, data, datalen);
	arc4random_prng_put(prng);
}

#ifdef _ARC4RANDOM_TEST

#include <sys/wait.h>

#include <err.h>
#include <stdio.h>

int
main(int argc __unused, char **argv __unused)
{
	unsigned char gubbish[] = "random gubbish";
	const uint8_t zero64[64] = {0};
	uint8_t buf[2048];
	unsigned i, a, n;

	/* Test arc4random: should not be deterministic.  */
	if (printf("arc4random: %08"PRIx32"\n", arc4random()) < 0)
		err(1, "printf");

	/* Test stirring: should definitely not be deterministic.  */
	arc4random_stir();

	/* Test small buffer.  */
	arc4random_buf(buf, 8);
	if (printf("arc4randombuf small:") < 0)
		err(1, "printf");
	for (i = 0; i < 8; i++)
		if (printf(" %02x", buf[i]) < 0)
			err(1, "printf");
	if (printf("\n") < 0)
		err(1, "printf");

	/* Test addrandom: should not make the rest deterministic.  */
	arc4random_addrandom(gubbish, sizeof gubbish);

	/* Test large buffer.  */
	arc4random_buf(buf, sizeof buf);
	if (printf("arc4randombuf_large:") < 0)
		err(1, "printf");
	for (i = 0; i < sizeof buf; i++)
		if (printf(" %02x", buf[i]) < 0)
			err(1, "printf");
	if (printf("\n") < 0)
		err(1, "printf");

	/* Test misaligned small and large.  */
	for (a = 0; a < 64; a++) {
		for (n = a; n < sizeof buf; n++) {
			(void)memset(buf, 0, sizeof buf);
			arc4random_buf(buf, n - a);
			if (memcmp(buf + n - a, zero64, a) != 0)
				errx(1, "arc4random buffer overflow 0");

			(void)memset(buf, 0, sizeof buf);
			arc4random_buf(buf + a, n - a);
			if (memcmp(buf, zero64, a) != 0)
				errx(1, "arc4random buffer overflow 1");

			if ((2*a) <= n) {
				(void)memset(buf, 0, sizeof buf);
				arc4random_buf(buf + a, n - a - a);
				if (memcmp(buf + n - a, zero64, a) != 0)
					errx(1,
					    "arc4random buffer overflow 2");
			}
		}
	}

	/* Test fork-safety.  */
    {
	pid_t pid, rpid;
	int status;

	pid = fork();
	switch (pid) {
	case -1:
		err(1, "fork");
	case 0:
		_exit(arc4random_prng_get()->arc4_seeded);
	default:
		rpid = waitpid(pid, &status, 0);
		if (rpid == -1)
			err(1, "waitpid");
		if (rpid != pid)
			errx(1, "waitpid returned wrong pid"
			    ": %"PRIdMAX" != %"PRIdMAX,
			    (intmax_t)rpid,
			    (intmax_t)pid);
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				errx(1, "child exited with %d",
				    WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			errx(1, "child terminated on signal %d",
			    WTERMSIG(status));
		} else {
			errx(1, "child died mysteriously: %d", status);
		}
	}
    }

	/* XXX Test multithreaded fork safety...?  */

	return 0;
}
#endif
