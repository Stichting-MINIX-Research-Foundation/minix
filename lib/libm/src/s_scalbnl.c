/*	$NetBSD: s_scalbnl.c,v 1.9 2013/05/20 19:40:09 joerg Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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
__RCSID("$NetBSD: s_scalbnl.c,v 1.9 2013/05/20 19:40:09 joerg Exp $");

#include "namespace.h"

#include <float.h>
#include <math.h>
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE

#ifdef _LP64
long double
scalbnl(long double x, int n)
{
	return scalblnl(x, n);
}
#else
__strong_alias(_scalbnl, _scalblnl)
#endif

__weak_alias(scalbnl, _scalbnl)
__weak_alias(scalblnl, _scalblnl)
__weak_alias(ldexpl, _scalbnl)

#if LDBL_MANT_DIG == 64
#define	FROM_UNDERFLOW	0x1p65L
#define	TO_UNDERFLOW	0x1p-65L
#elif LDBL_MANT_DIG == 113
#define	FROM_UNDERFLOW	0x1p114L
#define	TO_UNDERFLOW	0x1p-114L
#else
#error Unsupported long double format
#endif

long double
scalblnl(long double x, long n)
{
	union ieee_ext_u u;

	/* Trivial cases first */
	if (n == 0 || x == 0.0L)
		return x;

	u.extu_ld = x;

	/* NaN and infinite don't change either, but trigger exception */
	if (u.extu_ext.ext_exp == EXT_EXP_INFNAN)
		return x + x;

	/* Protect against integer overflow in calculation of new exponent */
	if (n > LDBL_MAX_EXP - LDBL_MIN_EXP + LDBL_MANT_DIG)
		goto overflow;
	if (n < LDBL_MIN_EXP - LDBL_MAX_EXP - LDBL_MANT_DIG)
		goto underflow;

	/* Scale denormalized numbers slightly, so that they are normal */
	if (u.extu_ext.ext_exp == 0) {
		u.extu_ld *= FROM_UNDERFLOW;
		n -= LDBL_MANT_DIG + 1;
	}

	n += u.extu_ext.ext_exp;
	if (n >= LDBL_MAX_EXP + EXT_EXP_BIAS)
		goto overflow;
	/* Positive exponent (incl. bias) means normal result */
	if (n > 0) {
		u.extu_ext.ext_exp = n;
		return u.extu_ld;
	}
	/* Shift the exponent and let the multiply below handle subnormal */
	n += LDBL_MANT_DIG + 1;
	if (n <= 0)
		goto underflow;
	u.extu_ext.ext_exp = n;
	return u.extu_ld * TO_UNDERFLOW;

underflow:
	return LDBL_MIN * copysignl(LDBL_MIN, x);

overflow:
	return LDBL_MAX * copysignl(LDBL_MAX, x);
}

#endif
