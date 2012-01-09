/*	$NetBSD: arc4random.c,v 1.29 2011/11/29 13:16:27 drochner Exp $	*/

/*-
 * Copyright (c) 2002, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Thor Lancelot Simon.
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

/*-
 * THE BEER-WARE LICENSE
 *
 * <dan@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff.  If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *
 * Dan Moschuk
 *
 * $FreeBSD: src/sys/libkern/arc4random.c,v 1.9 2001/08/30 12:30:58 bde Exp $
 */

#include <sys/cdefs.h>

#ifdef _KERNEL
#include "rnd.h"
#else
#define NRND 0
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/kernel.h>
#endif
#include <sys/systm.h>

#ifdef _KERNEL
#include <sys/mutex.h>
#include <sys/rngtest.h>
#else
#define mutex_spin_enter(x) ;
#define mutex_spin_exit(x) ;
#define mutex_init(x, y, z) ;
#endif

#include <lib/libkern/libkern.h>

#if NRND > 0
#include <sys/rnd.h>
#include <dev/rnd_private.h>

static rndsink_t	rs;

#endif

/*
 * The best known attack that distinguishes RC4 output from a random
 * bitstream requires 2^25 bytes.  (see Paul and Preneel, Analysis of
 * Non-fortuitous Predictive States of the RC4 Keystream Generator.
 * INDOCRYPT 2003, pp52 – 67).
 *
 * However, we discard the first 1024 bytes of output, avoiding the
 * biases detected in this paper.  The best current attack that
 * can distinguish this "RC4[drop]" output seems to be Fleuhrer &
 * McGrew's attack which requires 2^30.6 bytes of output:
 * Fluhrer and McGrew, Statistical Analysis of the Alleged RC4
 * Keystream Generator. FSE 2000, pp19 – 30
 *
 * We begin trying to rekey at 2^24 bytes, and forcibly rekey at 2^29 bytes
 * even if the resulting key cannot be guaranteed to have full entropy.
 */
#define	ARC4_MAXBYTES		(16 * 1024 * 1024)
#define ARC4_HARDMAX		(512 * 1024 * 1024)
#define	ARC4_RESEED_SECONDS	300
#define	ARC4_KEYBYTES		16 /* 128 bit key */

#ifdef _STANDALONE
#define	time_uptime	1	/* XXX ugly! */
#endif /* _STANDALONE */

static u_int8_t arc4_i, arc4_j;
static int arc4_initialized = 0;
static int arc4_numbytes = 0;
static u_int8_t arc4_sbox[256];
static time_t arc4_nextreseed;

#ifdef _KERNEL
kmutex_t	arc4_mtx;
#endif

static inline u_int8_t arc4_randbyte(void);
static inline void arc4randbytes_unlocked(void *, size_t);
void _arc4randbytes(void *, size_t);
uint32_t _arc4random(void);

static inline void
arc4_swap(u_int8_t *a, u_int8_t *b)
{
	u_int8_t c;

	c = *a;
	*a = *b;
	*b = c;
}

/*
 * Stir our S-box.
 */
static void
arc4_randrekey(void *arg)
{
	u_int8_t key[256];
	int n, ask_for_more = 0;
#ifdef _KERNEL
#ifdef DIAGNOSTIC
#if 0	/* XXX rngtest_t is too large and could cause stack overflow */
	rngtest_t rt;
#endif
#endif
#endif
#if NRND > 0
	static int callback_pending;
	int r;
#endif

	/*
	 * The first time through, we must take what we can get,
	 * so schedule ourselves for callback no matter what.
	 */
	if (__predict_true(arc4_initialized)) {
		mutex_spin_enter(&arc4_mtx);
	}
#if NRND > 0	/* XXX without rnd, we will key from the stack, ouch! */
	else {
		ask_for_more = 1;
		r = rnd_extract_data(key, ARC4_KEYBYTES, RND_EXTRACT_ANY);
		goto got_entropy;
	}

	if (arg == NULL) {
		if (callback_pending) {
			if (arc4_numbytes > ARC4_HARDMAX) {
				printf("arc4random: WARNING, hit 2^29 bytes, "
				    "forcibly rekeying.\n");
				r = rnd_extract_data(key, ARC4_KEYBYTES,
				    RND_EXTRACT_ANY);
				rndsink_detach(&rs);
				callback_pending = 0;
				goto got_entropy;
			} else {
				mutex_spin_exit(&arc4_mtx);
				return;
			}
		}
		r = rnd_extract_data(key, ARC4_KEYBYTES, RND_EXTRACT_GOOD);
		if (r < ARC4_KEYBYTES) {
			ask_for_more = 1;
		}
	} else {
		ask_for_more = 0;
		callback_pending = 0;
		if (rs.len != ARC4_KEYBYTES) {
			panic("arc4_randrekey: rekey callback bad length");
		}
		memcpy(key, rs.data, rs.len);
		memset(rs.data, 0, rs.len);
	}

got_entropy:

	if (!ask_for_more) {
		callback_pending = 0;
	} else if (!callback_pending) {
		callback_pending = 1;
		strlcpy(rs.name, "arc4random", sizeof(rs.name));
		rs.cb = arc4_randrekey;
		rs.arg = &rs;
		rs.len = ARC4_KEYBYTES;
		rndsink_attach(&rs);
	}
#endif
	/*
	 * If it's the first time, or we got a good key, actually rekey.
	 */
	if (!ask_for_more || !arc4_initialized) {
		for (n = ARC4_KEYBYTES; n < sizeof(key); n++)
				key[n] = key[n % ARC4_KEYBYTES];

		for (n = 0; n < 256; n++) {
			arc4_j = (arc4_j + arc4_sbox[n] + key[n]) % 256;
			arc4_swap(&arc4_sbox[n], &arc4_sbox[arc4_j]);
		}
		arc4_i = arc4_j;

		memset(key, 0, sizeof(key));
		/*
		 * Throw away the first N words of output, as suggested in the
		 * paper "Weaknesses in the Key Scheduling Algorithm of RC4"
		 * by Fluher, Mantin, and Shamir.  (N = 256 in our case.)
		 */
		for (n = 0; n < 256 * 4; n++)
			arc4_randbyte();

		/* Reset for next reseed cycle. */
		arc4_nextreseed = time_uptime + ARC4_RESEED_SECONDS;
		arc4_numbytes = 0;
#ifdef _KERNEL
#ifdef DIAGNOSTIC
#if 0	/* XXX rngtest_t is too large and could cause stack overflow */
		/*
		 * Perform the FIPS 140-2 statistical RNG test; warn if our
		 * output has such poor quality as to fail the test.
		 */
		arc4randbytes_unlocked(rt.rt_b, sizeof(rt.rt_b));
		strlcpy(rt.rt_name, "arc4random", sizeof(rt.rt_name));
		if (rngtest(&rt)) {
			/* rngtest will scream to the console. */
			arc4_nextreseed = time_uptime;
			arc4_numbytes = ARC4_MAXBYTES;
			/* XXX should keep old context around, *NOT* use new */
		}
#endif
#endif
#endif
	}
	if (__predict_true(arc4_initialized)) {
		mutex_spin_exit(&arc4_mtx);
	}
}

/*
 * Initialize our S-box to its beginning defaults.
 */
static void
arc4_init(void)
{
	int n;

	mutex_init(&arc4_mtx, MUTEX_DEFAULT, IPL_VM);
	arc4_i = arc4_j = 0;
	for (n = 0; n < 256; n++)
		arc4_sbox[n] = (u_int8_t) n;

	arc4_randrekey(NULL);
	arc4_initialized = 1;
}

/*
 * Generate a random byte.
 */
static inline u_int8_t
arc4_randbyte(void)
{
	u_int8_t arc4_t;

	arc4_i = (arc4_i + 1) % 256;
	arc4_j = (arc4_j + arc4_sbox[arc4_i]) % 256;

	arc4_swap(&arc4_sbox[arc4_i], &arc4_sbox[arc4_j]);

	arc4_t = (arc4_sbox[arc4_i] + arc4_sbox[arc4_j]) % 256;
	return arc4_sbox[arc4_t];
}

static inline void
arc4randbytes_unlocked(void *p, size_t len)
{
	u_int8_t *buf = (u_int8_t *)p;
	size_t i;

	for (i = 0; i < len; buf[i] = arc4_randbyte(), i++)
		continue;
}

void
_arc4randbytes(void *p, size_t len)
{
	/* Initialize array if needed. */
	if (!arc4_initialized) {
		arc4_init();
		/* avoid conditionalizing locking */
		return arc4randbytes_unlocked(p, len);
	}
	mutex_spin_enter(&arc4_mtx);
	arc4randbytes_unlocked(p, len);
	arc4_numbytes += len;
	mutex_spin_exit(&arc4_mtx);
	if ((arc4_numbytes > ARC4_MAXBYTES) ||
	    (time_uptime > arc4_nextreseed)) {
		arc4_randrekey(NULL);
	}
}

u_int32_t
_arc4random(void)
{
        u_int32_t ret;
        u_int8_t *retc;

        retc = (u_int8_t *)&ret;

        _arc4randbytes(retc, sizeof(u_int32_t));
        return ret;
}
