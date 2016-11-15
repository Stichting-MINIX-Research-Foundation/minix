/* $NetBSD: dtv_math.c,v 1.5 2011/08/09 01:42:24 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Alan Barrett <apb@NetBSD.org>
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
__KERNEL_RCSID(0, "$NetBSD: dtv_math.c,v 1.5 2011/08/09 01:42:24 jmcneill Exp $");

#include <sys/types.h>
#include <sys/bitops.h>
#include <sys/module.h>

#include <dev/dtv/dtv_math.h>

/*
 * dtv_intlog10 -- return an approximation to log10(x) * 1<<24,
 * using integer arithmetic.
 *
 * As a special case, returns 0 when x == 0.  The mathematical
 * result is -infinity.
 *
 * This function uses 0.5 + x/2 - 1/x as an approximation to
 * log2(x) for x in the range [1.0, 2.0], and scales the input value
 * to fit this range.  The resulting error is always better than
 * 0.2%.
 *
 * Here's a table of the desired and actual results, as well
 * as the absolute and relative errors, for several values of x.
 *
 *           x     desired      actual     err_abs err_rel
 *           0           0           0          +0 +0.00000
 *           1           0           0          +0 +0.00000
 *           2     5050445     5050122        -323 -0.00006
 *           3     8004766     7996348       -8418 -0.00105
 *           4    10100890    10100887          -3 -0.00000
 *           5    11726770    11741823      +15053 +0.00128
 *           6    13055211    13046470       -8741 -0.00067
 *           7    14178392    14158860      -19532 -0.00138
 *           8    15151335    15151009        -326 -0.00002
 *           9    16009532    16028061      +18529 +0.00116
 *          10    16777216    16792588      +15372 +0.00092
 *          11    17471670    17475454       +3784 +0.00022
 *          12    18105656    18097235       -8421 -0.00047
 *          13    18688868    18672077      -16791 -0.00090
 *          14    19228837    19209625      -19212 -0.00100
 *          15    19731537    19717595      -13942 -0.00071
 *          16    20201781    20201774          -7 -0.00000
 *          20    21827661    21842710      +15049 +0.00069
 *          24    23156102    23147357       -8745 -0.00038
 *          30    24781982    24767717      -14265 -0.00058
 *          40    26878106    26893475      +15369 +0.00057
 *          60    29832427    29818482      -13945 -0.00047
 *         100    33554432    33540809      -13623 -0.00041
 *        1000    50331648    50325038       -6610 -0.00013
 *       10000    67108864    67125985      +17121 +0.00026
 *      100000    83886080    83875492      -10588 -0.00013
 *     1000000   100663296   100652005      -11291 -0.00011
 *    10000000   117440512   117458739      +18227 +0.00016
 *   100000000   134217728   134210175       -7553 -0.00006
 *  1000000000   150994944   150980258      -14686 -0.00010
 *  4294967295   161614248   161614192         -56 -0.00000
 */
uint32_t
dtv_intlog10(uint32_t x)
{
	uint32_t ilog2x;
	uint32_t t;
	uint32_t t1;

	if (__predict_false(x == 0))
		return 0;

	/*
	 * find ilog2x = floor(log2(x)), as an integer in the range [0,31].
	 */
	ilog2x = ilog2(x);

	/*
	 * Set "t" to the result of shifting x left or right
	 * until the most significant bit that was actually set
	 * moves into the 1<<24 position.
	 *
	 * Now we can think of "t" as representing
	 * x / 2**(floor(log2(x))),
	 * as a fixed-point value with 8 integer bits and 24 fraction bits.
	 *
	 * This value is in the semi-closed interval [1.0, 2.0)
	 * when interpreting it as a fixed-point number, or in the
	 * interval [0x01000000, 0x01ffffff] when examining the
	 * underlying uint32_t representation.
	 */
	t = (ilog2x > 24 ? x >> (ilog2x - 24) : x << (24 - ilog2x));

	/*
	 * Calculate "t1 = 1 / t" in the 8.24 fixed-point format.
	 * This value is in the interval [0.5, 1.0]
	 * when interpreting it as a fixed-point number, or in the
	 * interval [0x00800000, 0x01000000] when examining the
	 * underlying uint32_t representation.
	 *
	 */
	t1 = ((uint64_t)1 << 48) / t;

	/*
	 * Calculate "t = ilog2x + t/2 - t1 + 0.5" in the 8.24
	 * fixed-point format.
	 *
	 * If x is a power of 2, then t is now exactly equal to log2(x)
	 * when interpreting it as a fixed-point number, or exactly
	 * log2(x) << 24 when examining the underlying uint32_t
	 * representation.
	 *
	 * If x is not a power of 2, then t is the result of
	 * using the function x/2 - 1/x + 0.5 as an approximation for
	 * log2(x) for x in the range [1, 2], and scaling both the
	 * input and the result by the appropriate number of powers of 2.
	 */
	t = (ilog2x << 24) + (t >> 1) - t1 + (1 << 23);

	/*
	 * Multiply t by log10(2) to get the final result.
	 *
	 * log10(2) is approximately 643/2136  We divide before
	 * multiplying to avoid overflow.
	 */
	return t / 2136 * 643;
}

#ifdef _KERNEL
MODULE(MODULE_CLASS_MISC, dtv_math, NULL);

static int
dtv_math_modcmd(modcmd_t cmd, void *opaque)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
#endif

#ifdef TEST_DTV_MATH
/*
 * To test:
 *	cc -DTEST_DTV_MATH ./dtv_math.c -lm -o ./a.out && ./a.out
 */

#include <stdio.h>
#include <inttypes.h>
#include <math.h>

int
main(void)
{
	uint32_t xlist[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
			14, 15, 16, 20, 24, 30, 40, 60, 100, 1000, 10000,
			100000, 1000000, 10000000, 100000000, 1000000000,
			0xffffffff};
	int i;

	printf("%11s %11s %11s %11s %s\n",
		"x", "desired", "actual", "err_abs", "err_rel");
	for (i = 0; i < __arraycount(xlist); i++)
	{
		uint32_t x = xlist[i];
		uint32_t desired = (uint32_t)(log10((double)x)
						* (double)(1<<24));
		uint32_t actual = dtv_intlog10(x);
		int32_t err_abs = actual - desired;
		double err_rel = (err_abs == 0 ? 0.0
				: err_abs / (double)actual);

		printf("%11"PRIu32" %11"PRIu32" %11"PRIu32
			" %+11"PRId32" %+.5f\n",
			x, desired, actual, err_abs, err_rel);
	}
	return 0;
}

#endif /* TEST_DTV_MATH */
