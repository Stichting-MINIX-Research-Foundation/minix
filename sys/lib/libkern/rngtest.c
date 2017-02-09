/*	$NetBSD: rngtest.c,v 1.2 2011/11/25 12:45:00 joerg Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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

/* fips140.c	1.5 (Qualcomm) 02/09/02 */
/*
This software is free for commercial and non-commercial use
subject to the following conditions.

Copyright remains vested in QUALCOMM Incorporated, and Copyright
notices in the code are not to be removed.  If this package is used in
a product, QUALCOMM should be given attribution as the author this
software.  This can be in the form of a textual message at program
startup or in documentation (online or textual) provided with the
package.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the
   distribution.

3. All advertising materials mentioning features or use of this
   software must display the following acknowledgement:  This product
   includes software developed by QUALCOMM Incorporated.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

The license and distribution terms for any publically available version
or derivative of this code cannot be changed, that is, this code cannot
simply be copied and put under another distribution license including
the GNU Public License.
*/

/* Run FIPS 140 statistical tests on a file */

/* written by Greg Rose, Copyright C 2000 QUALCOMM Incorporated */

/*
 * Modified for in-kernel use (API adjustments, conversion from
 * floating to fixed-point chi-sq computation) by Thor Lancelot
 * Simon.
 *
 * A comment on appropriate use of this test and the infamous FIPS 140
 * "continuous output test" (COT):  Both tests are very appropriate for
 * software interfaces to hardware implementations, and will quickly tell
 * you if any number of very bad things have happened to your RNG: perhaps
 * it has come disconnected from the rest of the system, somehow, and you
 * are getting only unconditioned bus noise (read: clock edges from the
 * loudest thing in your system).  Perhaps it has ceased to latch a shift
 * register and is feeding you the same data over and over again.  Perhaps
 * it is not really random at all but was sold to you as such.  Perhaps it
 * is not actually *there* (Intel chipset RNG anyone?) but claims to be,
 * and is feeding you 01010101 on every register read.
 *
 * However, when applied to software RNGs, the situation is quite different.
 * Most software RNGs use a modern hash function or cipher as an output
 * stage.  The resulting bitstream assuredly *should* pass both the
 * "continuous output" (no two consecutive samples identical) and
 * statistical tests: if it does not, the cryptographic primitive or its
 * implementation is terribly broken.
 *
 * There is still value to this test: it will tell you if you inadvertently
 * terribly break your implementation of the software RNG.  Which is a thing
 * that has in fact happened from time to time, even to the careful (or
 * paranoid).  But it will not tell you if there is a problem with the
 * _input_ to that final cryptographic primitive -- the bits that are hashed
 * or the key to the cipher -- and if an adversary can find one, you're
 * still toast.
 *
 * The situation is -- sadly -- similar with hardware RNGs that are
 * certified to one of the standards such as X9.31 or SP800-90.  In these
 * cases the hardware vendor has hidden the actual random bitstream behind
 * a hardware cipher/hash implementation that should, indeed, produce good
 * quality random numbers that pass will pass this test -- whether the
 * underlying bitstream is trustworthy or not.
 *
 * However, this test (and the COT) will still probably tell you if the
 * thing fell off the bus, etc.  Which is a thing that has in fact
 * happened from time to time, even to the fully certified...
 *
 * This module does not (yet?) implement the Continuous Output Test.  When
 * I call that test "infamous", it's because it obviously reduces the
 * backtracking resistance of any implementation that includes it -- the
 * implementation has to store the entire previous RNG output in order to
 * perform the required comparison; not just periodically but all the time
 * when operating at all.  Nonetheless, it has obvious value for
 * hardware implementations where it will quickly and surely detect a
 * severe failure; but as of this writing several of the latest comments
 * on SP800-90 recommend removing any requirement for the COT and my
 * personal tendency is to agree.  It's easy to add if you really need it.
 *
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/rngtest.h>

#include <lib/libkern/libkern.h>

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rngtest.c,v 1.2 2011/11/25 12:45:00 joerg Exp $");

#ifndef _KERNEL
static inline int
printf(const char * __restrict format, ...)
{
	return 0;	/* XXX no standard way to do output in libkern? */
}
#endif

int bitnum = 0;

const int minrun[7] = {0, 2315, 1114, 527, 240, 103, 103};
const int maxrun[7] = {0, 2685, 1386, 723, 384, 209, 209};
#define LONGRUN	26
#define MINONES 9725
#define MAXONES 10275
#define MINPOKE 2.16
#define MAXPOKE 46.17
#define PRECISION 100000

const int longrun = LONGRUN;
const int minones = MINONES;
const int maxones = MAXONES;
const long long minpoke = (MINPOKE * PRECISION);
const long long maxpoke = (MAXPOKE * PRECISION);

/* Population count of 1's in a byte */
const unsigned char Popcount[] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

/* end of run */
static void
endrun(rngtest_t *const rc, const int last, int run)
{
	if (run >= longrun) {
		printf("Kernel RNG \"%s\" long run test FAILURE: "
		       "Run of %d %ds found\n", rc->rt_name, run, last);
		++rc->rt_nerrs;
	}
	if (run > 6)
		run = 6;
	++rc->rt_runs[last][run];
}

int
rngtest(rngtest_t *const rc)
{
	int i;
	uint8_t *p;
	int c;
	long long X;
	int last;
	int run;

	/* Enforce sanity for most members of the context */
	memset(rc->rt_poker, 0, sizeof(rc->rt_poker));
	memset(rc->rt_runs, 0, sizeof(rc->rt_runs));
	rc->rt_nerrs = 0;
	rc->rt_name[sizeof(rc->rt_name) - 1] = '\0';

	/* monobit test */
	for (p = rc->rt_b, c = 0; p < &rc->rt_b[sizeof rc->rt_b]; ++p)
		c += Popcount[*p];
	if (c <= minones || maxones <= c) {
		printf("Kernel RNG \"%s\" monobit test FAILURE: %d ones\n",
		       rc->rt_name, c);
		++rc->rt_nerrs;
	}
	/* poker test */
	for (p = rc->rt_b; p < &rc->rt_b[sizeof rc->rt_b]; ++p) {
		++rc->rt_poker[*p & 0xF];
		++rc->rt_poker[(*p >> 4) & 0xF];
	}
	for (X = i = 0; i < 16; ++i) {
		X += rc->rt_poker[i] * rc->rt_poker[i];
	}
	X *= PRECISION;
	X = 16 * X / 5000 - 5000 * PRECISION;
	if (X <= minpoke || maxpoke <= X) {
		printf("Kernel RNG \"%s\" poker test failure: "
		       "parameter X = %lld.%lld\n", rc->rt_name,
		       (X / PRECISION), (X % PRECISION));
		++rc->rt_nerrs;
	}
	/* runs test */
	last = (rc->rt_b[0] >> 7) & 1;
	run = 0;
	for (p = rc->rt_b; p < &rc->rt_b[sizeof rc->rt_b]; ++p) {
		c = *p;
		for (i = 7; i >= 0; --i) {
			if (((c >> i) & 1) != last) {
				endrun(rc, last, run);
				run = 0;
				last = (c >> i) & 1;
			}
			++run;
		}
	}
	endrun(rc, last, run);

	for (run = 1; run <= 6; ++run) {
		for (last = 0; last <= 1; ++last) {
			if (rc->rt_runs[last][run] <= minrun[run]) {
				printf("Kernel RNG \"%s\" runs test FAILURE: "
				       "too few runs of %d %ds (%d < %d)\n",
				       rc->rt_name, run, last,
				       rc->rt_runs[last][run], minrun[run]);
				++rc->rt_nerrs;
			} else if (rc->rt_runs[last][run] >= maxrun[run]) {
				printf("Kernel RNG \"%s\" runs test FAILURE: "
				       "too many runs of %d %ds (%d > %d)\n",
				       rc->rt_name, run, last,
				       rc->rt_runs[last][run], maxrun[run]);
				++rc->rt_nerrs;
			}
		}
	}
	memset(rc->rt_b, 0, sizeof(rc->rt_b));
	return rc->rt_nerrs;
}
