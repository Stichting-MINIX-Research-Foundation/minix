/*	$NetBSD: randomid.c,v 1.13 2009/01/11 02:46:27 christos Exp $	*/
/*	$KAME: ip6_id.c,v 1.8 2003/09/06 13:41:06 itojun Exp $	*/
/*	$OpenBSD: ip_id.c,v 1.6 2002/03/15 18:19:52 millert Exp $	*/

/*
 * Copyright (C) 2003 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright 1998 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Theo de Raadt <deraadt@openbsd.org> came up with the idea of using
 * such a mathematical system to generate more random (yet non-repeating)
 * ids to solve the resolver/named problem.  But Niels designed the
 * actual system based on the constraints.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * seed = random (bits - 1) bit
 * n = prime, g0 = generator to n,
 * j = random so that gcd(j,n-1) == 1
 * g = g0^j mod n will be a generator again.
 *
 * X[0] = random seed.
 * X[n] = a*X[n-1]+b mod m is a Linear Congruential Generator
 * with a = 7^(even random) mod m,
 *      b = random with gcd(b,m) == 1
 *      m = constant and a maximal period of m-1.
 *
 * The transaction id is determined by:
 * id[n] = seed xor (g^X[n] mod n)
 *
 * Effectivly the id is restricted to the lower (bits - 1) bits, thus
 * yielding two different cycles by toggling the msb on and off.
 * This avoids reuse issues caused by reseeding.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: randomid.c,v 1.13 2009/01/11 02:46:27 christos Exp $");
#endif

#include "namespace.h"

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <randomid.h>

#ifdef __weak_alias
__weak_alias(randomid,_randomid)
__weak_alias(randomid_new,_randomid_new)
__weak_alias(randomid_delete,_randomid_delete)
#endif

struct randomconf {
	const int	rc_bits; /* resulting bits */
	const u_int32_t rc_max;	/* Uniq cycle, avoid blackjack prediction */
	const u_int32_t rc_gen;	/* Starting generator */
	const u_int32_t rc_n;	/* ru_n: prime, ru_n - 1: product of pfacts[] */
	const u_int32_t rc_agen; /* determine ru_a as ru_agen^(2*rand) */
	const u_int32_t rc_m;	/* ru_m = 2^x*3^y */
	const u_int32_t rc_pfacts[4];	/* factors of ru_n */
	const int	rc_skip;	/* skip values */
};

struct randomid_ctx {
	struct randomconf *ru_conf;
#define ru_bits		ru_conf->rc_bits
#define ru_max		ru_conf->rc_max
#define ru_gen		ru_conf->rc_gen
#define ru_n		ru_conf->rc_n
#define ru_agen		ru_conf->rc_agen
#define ru_m		ru_conf->rc_m
#define ru_pfacts	ru_conf->rc_pfacts
#define ru_skip		ru_conf->rc_skip
	long ru_out;		/* Time after wich will be reseeded */
	u_int32_t ru_counter;
	u_int32_t ru_msb;

	u_int32_t ru_x;
	u_int32_t ru_seed, ru_seed2;
	u_int32_t ru_a, ru_b;
	u_int32_t ru_g;
	time_t ru_reseed;
};

static struct randomconf randomconf[] = {
  {
	32,			/* resulting bits */
	1000000000,		/* Uniq cycle, avoid blackjack prediction */
	2,			/* Starting generator */
	2147483629,		/* RU_N-1 = 2^2*3^2*59652323 */
	7,			/* determine ru_a as RU_AGEN^(2*rand) */
	1836660096,		/* RU_M = 2^7*3^15 - don't change */
	{ 2, 3, 59652323, 0 },	/* factors of ru_n */
	3,			/* skip values */
  },
  {
	20,			/* resulting bits */
	200000,			/* Uniq cycle, avoid blackjack prediction */
	2,			/* Starting generator */
	524269,			/* RU_N-1 = 2^2*3^2*14563 */
	7,			/* determine ru_a as RU_AGEN^(2*rand) */
	279936,			/* RU_M = 2^7*3^7 - don't change */
	{ 2, 3, 14563, 0 },	/* factors of ru_n */
	3,			/* skip values */
  },
  {
	16,			/* resulting bits */
	30000,			/* Uniq cycle, avoid blackjack prediction */
	2,			/* Starting generator */
	32749,			/* RU_N-1 = 2^2*3*2729 */
	7,			/* determine ru_a as RU_AGEN^(2*rand) */
	31104,			/* RU_M = 2^7*3^5 - don't change */
	{ 2, 3, 2729, 0 },	/* factors of ru_n */
	0,			/* skip values */
  },
  {
	.rc_bits = -1,		/* termination */
  },
};

static u_int32_t pmod(u_int32_t, u_int32_t, u_int32_t);
static void initid(struct randomid_ctx *);

struct randomid_ctx *randomid_new(int, long);
void randomid_delete(struct randomid_ctx *);
u_int32_t randomid(struct randomid_ctx *);

/*
 * Do a fast modular exponation, returned value will be in the range
 * of 0 - (mod-1)
 */

static u_int32_t
pmod(u_int32_t gen, u_int32_t expo, u_int32_t mod)
{
	u_int64_t s, t, u;

	s = 1;
	t = gen;
	u = expo;

	while (u) {
		if (u & 1)
			s = (s * t) % mod;
		u >>= 1;
		t = (t * t) % mod;
	}
	return ((u_int32_t)s & UINT32_MAX);
}

/*
 * Initalizes the seed and chooses a suitable generator. Also toggles
 * the msb flag. The msb flag is used to generate two distinct
 * cycles of random numbers and thus avoiding reuse of ids.
 *
 * This function is called from id_randomid() when needed, an
 * application does not have to worry about it.
 */
static void
initid(struct randomid_ctx *p)
{
	u_int32_t j, i;
	int noprime = 1;
	struct timeval tv;

	p->ru_x = arc4random() % p->ru_m;

	/* (bits - 1) bits of random seed */
	p->ru_seed = arc4random() & (~0U >> (32 - p->ru_bits + 1));
	p->ru_seed2 = arc4random() & (~0U >> (32 - p->ru_bits + 1));

	/* Determine the LCG we use */
	p->ru_b = (arc4random() & (~0U >> (32 - p->ru_bits))) | 1;
	p->ru_a = pmod(p->ru_agen,
	    (arc4random() & (~0U >> (32 - p->ru_bits))) & (~1U), p->ru_m);
	while (p->ru_b % 3 == 0)
		p->ru_b += 2;

	j = arc4random() % p->ru_n;

	/*
	 * Do a fast gcd(j, RU_N - 1), so we can find a j with
	 * gcd(j, RU_N - 1) == 1, giving a new generator for
	 * RU_GEN^j mod RU_N
	 */
	while (noprime) {
		for (i = 0; p->ru_pfacts[i] > 0; i++)
			if (j % p->ru_pfacts[i] == 0)
				break;

		if (p->ru_pfacts[i] == 0)
			noprime = 0;
		else
			j = (j + 1) % p->ru_n;
	}

	p->ru_g = pmod(p->ru_gen, j, p->ru_n);
	p->ru_counter = 0;

	gettimeofday(&tv, NULL);
	p->ru_reseed = tv.tv_sec + p->ru_out;
	p->ru_msb = p->ru_msb ? 0 : (1U << (p->ru_bits - 1));
}

struct randomid_ctx *
randomid_new(int bits, long timeo)
{
	struct randomconf *conf;
	struct randomid_ctx *ctx;

	if (timeo < RANDOMID_TIMEO_MIN) {
		errno = EINVAL;
		return (NULL);
	}

	for (conf = randomconf; conf->rc_bits > 0; conf++) {
		if (bits == conf->rc_bits)
			break;
	}

	/* unsupported bits */
	if (bits != conf->rc_bits) {
		errno = ENOTSUP;
		return (NULL);
	}

	ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return (NULL);

	memset(ctx, 0, sizeof(*ctx));
	ctx->ru_conf = conf;
	ctx->ru_out = timeo;

	return (ctx);
}

void
randomid_delete(struct randomid_ctx *ctx)
{

	memset(ctx, 0, sizeof(*ctx));
	free(ctx);
}

u_int32_t
randomid(struct randomid_ctx *p)
{
	int i, n;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	if (p->ru_counter >= p->ru_max || tv.tv_sec > p->ru_reseed)
		initid(p);

	/* Skip a random number of ids */
	if (p->ru_skip) {
		n = arc4random() & p->ru_skip;
		if (p->ru_counter + n >= p->ru_max)
			initid(p);
	} else
		n = 0;

	for (i = 0; i <= n; i++) {
		/* Linear Congruential Generator */
		p->ru_x = (u_int32_t)(((u_int64_t)p->ru_a * p->ru_x + p->ru_b) % p->ru_m);
	}

	p->ru_counter += i;

	return (p->ru_seed ^ pmod(p->ru_g, p->ru_seed2 + p->ru_x, p->ru_n)) |
	    p->ru_msb;
}
