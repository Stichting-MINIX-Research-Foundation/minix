/*-
 * Copyright (c) 2007 Steven G. Kargl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/cdefs.h>
#if 0
__FBSDID("$FreeBSD: head/lib/msun/src/e_sqrtl.c 176720 2008-03-02 01:47:58Z das $");
#endif
__RCSID("$NetBSD: e_sqrtl.c,v 1.4 2013/11/22 20:15:06 martin Exp $");

#include <machine/ieee.h>
#include <float.h>

#include "math.h"
#include "math_private.h"

#ifdef __HAVE_LONG_DOUBLE

#ifdef HAVE_FENV_H
#include <fenv.h>
#endif

#ifdef LDBL_IMPLICIT_NBIT
#define	LDBL_NBIT	0
#endif

#ifdef HAVE_FENV_H

/* Return (x + ulp) for normal positive x. Assumes no overflow. */
static inline long double
inc(long double x)
{
	union ieee_ext_u ux = { .extu_ld = x, };

	if (++ux.extu_fracl == 0) {
		if (++ux.extu_frach == 0) {
			ux.extu_exp++;
			ux.extu_frach |= LDBL_NBIT;
		}
	}
	return (ux.extu_ld);
}

/* Return (x - ulp) for normal positive x. Assumes no underflow. */
static inline long double
dec(long double x)
{
	union ieee_ext_u ux = { .extu_ld = x, };

	if (ux.extu_fracl-- == 0) {
		if (ux.extu_frach-- == LDBL_NBIT) {
			ux.extu_exp--;
			ux.extu_frach |= LDBL_NBIT;
		}
	}
	return (ux.extu_ld);
}

/*
 * This is slow, but simple and portable. You should use hardware sqrt
 * if possible.
 */

long double
__ieee754_sqrtl(long double x)
{
	union ieee_ext_u ux = { .extu_ld = x, };
	int k, r;
	long double lo, xn;
	fenv_t env;

	/* If x = NaN, then sqrt(x) = NaN. */
	/* If x = Inf, then sqrt(x) = Inf. */
	/* If x = -Inf, then sqrt(x) = NaN. */
	if (ux.extu_exp == LDBL_MAX_EXP * 2 - 1)
		return (x * x + x);

	/* If x = +-0, then sqrt(x) = +-0. */
	if ((ux.extu_frach | ux.extu_fracl | ux.extu_exp) == 0)
		return (x);

	/* If x < 0, then raise invalid and return NaN */
	if (ux.extu_sign)
		return ((x - x) / (x - x));

	feholdexcept(&env);

	if (ux.extu_exp == 0) {
		/* Adjust subnormal numbers. */
		ux.extu_ld *= 0x1.0p514;
		k = -514;
	} else {
		k = 0;
	}
	/*
	 * ux.extu_ld is a normal number, so break it into ux.extu_ld = e*2^n where
	 * ux.extu_ld = (2*e)*2^2k for odd n and ux.extu_ld = (4*e)*2^2k for even n.
	 */
	if ((ux.extu_exp - EXT_EXP_BIAS) & 1) {	/* n is even.     */
		k += ux.extu_exp - EXT_EXP_BIAS - 1; /* 2k = n - 2.   */
		ux.extu_exp = EXT_EXP_BIAS + 1;	/* ux.extu_ld in [2,4). */
	} else {
		k += ux.extu_exp - EXT_EXP_BIAS;	/* 2k = n - 1.   */
		ux.extu_exp = EXT_EXP_BIAS;	/* ux.extu_ld in [1,2). */
	}

	/*
	 * Newton's iteration.
	 * Split ux.extu_ld into a high and low part to achieve additional precision.
	 */
	xn = sqrt(ux.extu_ld);			/* 53-bit estimate of sqrtl(x). */
#if LDBL_MANT_DIG > 100
	xn = (xn + (ux.extu_ld / xn)) * 0.5;	/* 106-bit estimate. */
#endif
	lo = ux.extu_ld;
	ux.extu_fracl = 0;		/* Zero out lower bits. */
	lo = (lo - ux.extu_ld) / xn;	/* Low bits divided by xn. */
	xn = xn + (ux.extu_ld / xn);	/* High portion of estimate. */
	ux.extu_ld = xn + lo;		/* Combine everything. */
	ux.extu_exp += (k >> 1) - 1;

	feclearexcept(FE_INEXACT);
	r = fegetround();
	fesetround(FE_TOWARDZERO);	/* Set to round-toward-zero. */
	xn = x / ux.extu_ld;		/* Chopped quotient (inexact?). */

	if (!fetestexcept(FE_INEXACT)) { /* Quotient is exact. */
		if (xn == ux.extu_ld) {
			fesetenv(&env);
			return (ux.extu_ld);
		}
		/* Round correctly for inputs like x = y**2 - ulp. */
		xn = dec(xn);		/* xn = xn - ulp. */
	}

	if (r == FE_TONEAREST) {
		xn = inc(xn);		/* xn = xn + ulp. */
	} else if (r == FE_UPWARD) {
		ux.extu_ld = inc(ux.extu_ld);	/* ux.extu_ld = ux.extu_ld + ulp. */
		xn = inc(xn);		/* xn  = xn + ulp. */
	}
	ux.extu_ld = ux.extu_ld + xn;		/* Chopped sum. */
	feupdateenv(&env);	/* Restore env and raise inexact */
	ux.extu_exp--;
	return (ux.extu_ld);
}

#else

/*
 * No fenv support:
 * poor man's version: just use double
 */
long double
__ieee754_sqrtl(long double x)
{
	return __ieee754_sqrt((double)x);
}

#endif

#endif
