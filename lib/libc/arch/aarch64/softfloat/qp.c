/* $NetBSD: qp.c,v 1.1 2014/08/10 05:47:37 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include "milieu.h"
#include "softfloat.h"

/*
 * This file provides wrappers for the softfloat functions.  We can't use
 * invoke them directly since long double arguments are passed in FP/SIMD
 * as well as being returned in them while float128 arguments are passed
 * in normal registers.
 */

long double __addtf3(long double, long double);
long double __divtf3(long double, long double);
long double __modtf3(long double, long double);
long double __multf3(long double, long double);
long double __negtf2(long double);
long double __subtf3(long double, long double);

int __getf2(long double, long double);
int __lttf2(long double, long double);
int __gttf2(long double, long double);
int __letf2(long double, long double);
int __eqtf2(long double, long double);
int __netf2(long double, long double);
int __unordtf2(long double, long double);

double __trunctfdf2(long double);
float __trunctfsf2(long double);

long double __extendsftf2(float);
long double __extenddftf2(double);

long double __floatsitf(int32_t);
long double __floatditf(int64_t);

long double __floatunsitf(uint32_t);
long double __floatunditf(uint64_t);

int32_t __fixtfsi(long double);
int64_t __fixtfdi(long double);

uint32_t __fixuntfsi(long double);
uint64_t __fixuntfdi(long double);

#if 0
long double __floattitf(int128_t);
long double __floatuntitf(uint128_t);
int128_t __fixtfti(long double);
uint128_t __fixuntfti(long double);
#endif

union sf_ieee_flt_u {
	float fltu_f;
	float32 fltu_f32;
};

union sf_ieee_dbl_u {
	double dblu_d;
	float64 dblu_f64;
};

union sf_ieee_ldbl_u {
	long double ldblu_ld;
	float128 ldblu_f128;
};

long double
__addtf3(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };
	const union sf_ieee_ldbl_u c = {
	    .ldblu_f128 = float128_add(a.ldblu_f128, b.ldblu_f128)
	};

	return c.ldblu_ld;
}

long double
__divtf3(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };
	const union sf_ieee_ldbl_u c = {
	    .ldblu_f128 = float128_div(a.ldblu_f128, b.ldblu_f128)
	};

	return c.ldblu_ld;
}

long double
__multf3(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };
	const union sf_ieee_ldbl_u c = {
	    .ldblu_f128 = float128_mul(a.ldblu_f128, b.ldblu_f128)
	};

	return c.ldblu_ld;
}

long double
__negtf2(long double ld_a)
{
	const union sf_ieee_ldbl_u zero = { .ldblu_ld = 0.0 };
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = {
	    .ldblu_f128 = float128_div(zero.ldblu_f128, a.ldblu_f128)
	};

	return b.ldblu_ld;
}

long double
__subtf3(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };
	const union sf_ieee_ldbl_u c = {
	    .ldblu_f128 = float128_sub(a.ldblu_f128, b.ldblu_f128)
	};

	return c.ldblu_ld;
}

#if 0
int
__cmptf3(float128 *a, float128 *b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	if (float128_eq(*a, *b))
		return 0;

	if (float128_le(*a, *b))
		return 1;

	return 2;
}


/*
 * XXX 
 */
int
_Qp_cmpe(float128 *a, float128 *b)
{
	return _Qp_cmp(a, b);
}
#endif

int
__eqtf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	return float128_eq(a.ldblu_f128, b.ldblu_f128);
}

int
__getf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	return float128_le(b.ldblu_f128, a.ldblu_f128);
}

int
__gttf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	return float128_lt(b.ldblu_f128, a.ldblu_f128);
}

int
__letf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	return float128_le(a.ldblu_f128, b.ldblu_f128);
}

int
__lttf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	return float128_lt(a.ldblu_f128, b.ldblu_f128);
}

int
__netf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	return !float128_eq(a.ldblu_f128, b.ldblu_f128);
}

float
__trunctfsf2(long double ld_a)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_flt_u c = {
		.fltu_f32 = float128_to_float32(a.ldblu_f128),
	};

	return c.fltu_f;
}

double
__trunctfdf2(long double ld_a)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_dbl_u c = {
		.dblu_f64 = float128_to_float64(a.ldblu_f128),
	};

	return c.dblu_d;
}

int32_t
__fixtfsi(long double ld_a)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	return float128_to_int32_round_to_zero(a.ldblu_f128);
}

int64_t
__fixtfdi(long double ld_a)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };

	return float128_to_int64_round_to_zero(a.ldblu_f128);
}

#if 0
uint32_t
__fixuntfsi(long double ld_a)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };

	return float128_to_uint32_round_to_zero(a.ldblu_f128);
}

uint64_t
__fixuntfdi(long double ld_a)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };

	return float128_to_uint64_round_to_zero(a.ldblu_f128);
}
#endif

long double
__extendsftf2(float f_a)
{
	const union sf_ieee_flt_u a = { .fltu_f = f_a };
	const union sf_ieee_ldbl_u c = {
		.ldblu_f128 = float32_to_float128(a.fltu_f32)
	};

	return c.ldblu_ld;
}

long double
__extenddftf2(double d_a)
{
	const union sf_ieee_dbl_u a = { .dblu_d = d_a };
	const union sf_ieee_ldbl_u c = {
		.ldblu_f128 = float64_to_float128(a.dblu_f64)
	};

	return c.ldblu_ld;
}

long double
__floatunsitf(uint32_t a)
{
	const union sf_ieee_ldbl_u c = {
		.ldblu_f128 = int64_to_float128(a)
	};

	return c.ldblu_ld;
}

long double
__floatunditf(uint64_t a)
{
	union sf_ieee_ldbl_u c;
	const uint64_t msb64 = 1LL << 63;

	if (a & msb64) {
		static const union sf_ieee_ldbl_u two63 = {
			.ldblu_ld = 0x1.0p63
		};
		
		c.ldblu_f128 = int64_to_float128(a ^ msb64);
		c.ldblu_f128 = float128_add(c.ldblu_f128, two63.ldblu_f128);
	} else {
		c.ldblu_f128 = int64_to_float128(a);
	}
	return c.ldblu_ld;
}

long double
__floatsitf(int32_t a)
{
	const union sf_ieee_ldbl_u c = {
		.ldblu_f128 = int64_to_float128(a)
	};

	return c.ldblu_ld;
}

long double
__floatditf(int64_t a)
{
	const union sf_ieee_ldbl_u c = {
		.ldblu_f128 = int64_to_float128(a)
	};

	return c.ldblu_ld;
}

int
__unordtf2(long double ld_a, long double ld_b)
{
	const union sf_ieee_ldbl_u a = { .ldblu_ld = ld_a };
	const union sf_ieee_ldbl_u b = { .ldblu_ld = ld_b };

	/*
	 * The comparison is unordered if either input is a NaN.
	 * Test for this by comparing each operand with itself.
	 * We must perform both comparisons to correctly check for
	 * signalling NaNs.
	 */
	return 1 ^ (float128_eq(a.ldblu_f128, a.ldblu_f128) & float128_eq(b.ldblu_f128, b.ldblu_f128));
}
