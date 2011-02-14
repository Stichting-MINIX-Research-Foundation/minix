/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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

#ifndef _TGMATH_H_
#define	_TGMATH_H_

#include <math.h>
#include <complex.h>

/*
 * C99 Type-generic math (7.22)
 */
#ifdef __GNUC__
#define	__TG_CHOOSE(p, a, b)	__builtin_choose_expr((p), (a), (b))
#define	__TG_IS_EQUIV_TYPE_P(v, t)	\
	 __builtin_types_compatible_p(__typeof__(v), t)
#else
#error how does this compler do type-generic macros?
#endif

#define	__TG_IS_FCOMPLEX_P(t)	__TG_IS_EQUIV_TYPE_P(t, float complex)
#define	__TG_IS_DCOMPLEX_P(t)	__TG_IS_EQUIV_TYPE_P(t, double complex)
#define	__TG_IS_LCOMPLEX_P(t)	__TG_IS_EQUIV_TYPE_P(t, long double complex)
#define	__TG_IS_FLOAT_P(t)	__TG_IS_EQUIV_TYPE_P(t, float)
#define	__TG_IS_LDOUBLE_P(t)	__TG_IS_EQUIV_TYPE_P(t, long double)
#define	__TG_IS_FREAL_P(t)	(__TG_IS_FLOAT_P(t) || __TG_IS_FCOMPLEX_P(t))
#define	__TG_IS_LREAL_P(t)	(__TG_IS_LDOUBLE_P(t) || __TG_IS_LCOMPLEX_P(t))

#define	__TG_IS_COMPLEX_P(t)					\
	(__TG_IS_FCOMPLEX_P(t)					\
	 || __TG_IS_DCOMPLEX_P(t)				\
	 || __TG_IS_LCOMPLEX_P(t))

#define	__TG_GFN1(fn, a, ftype, ltype)				\
	__TG_CHOOSE(__TG_IS_##ftype##_P(a),			\
		    fn##f(a),					\
		    __TG_CHOOSE(__TG_IS_##ltype##_P(a),		\
				fn##l(a),			\
				fn(a)))

#define	__TG_GFN1x(fn, a, b, ftype, ltype)			\
	__TG_CHOOSE(__TG_IS_##ftype##_P(a),			\
		    fn##f((a), (b)),				\
		    __TG_CHOOSE(__TG_IS_##ltype##_P(a),		\
				fn##l((a), (b)),		\
				fn((a), (b))))

#define	__TG_GFN2(fn, a, b, ftype, ltype)			\
	__TG_CHOOSE(__TG_IS_##ftype##_P(a)			\
		    && __TG_IS_##ftype##_P(b),			\
		    fn##f((a), (b)),				\
		    __TG_CHOOSE(__TG_IS_##ltype##_P(a)		\
				|| __TG_IS_##ltype##_P(b),	\
				fn##l((a), (b)),		\
				fn((a), (b))))

#define	__TG_GFN2x(fn, a, b, c, ftype, ltype)			\
	__TG_CHOOSE(__TG_IS_##ftype##_P(a)			\
		    && __TG_IS_##ftype##_P(b),			\
		    fn##f((a), (b), (c)),			\
		    __TG_CHOOSE(__TG_IS_##ltype##_P(a)		\
				|| __TG_IS_##ltype##_P(b),	\
				fn##l((a), (b), (c)),		\
				fn((a), (b), (c))))

#define	__TG_GFN3(fn, a, b, c, ftype, ltype)			\
	__TG_CHOOSE(__TG_IS_##ftype##_P(a)			\
		    && __TG_IS_##ftype##_P(b)			\
		    && __TG_IS_##ftype##_P(c),			\
		    fn##f((a), (b), (c)),			\
		    __TG_CHOOSE(__TG_IS_##ltype##_P(a)		\
				|| __TG_IS_##ltype##_P(b)	\
				|| __TG_IS_##ltype##_P(c),	\
				fn##l((a), (b), (c)),		\
				fn((a), (b), (c))))


#define	__TG_CFN1(cfn, a)	__TG_GFN1(cfn, a, FREAL, LREAL)
#define	__TG_CFN2(cfn, a, b)	__TG_GFN2(cfn, a, b, FREAL, LREAL)

#define	__TG_FN1(fn, a)		__TG_GFN1(fn, a, FLOAT, LDOUBLE)
#define	__TG_FN1x(fn, a, b)	__TG_GFN1x(fn, a, b, FLOAT, LDOUBLE)
#define	__TG_FN2(fn, a, b)	__TG_GFN2(fn, a, b, FLOAT, LDOUBLE)
#define	__TG_FN2x(fn, a, b, c)	__TG_GFN2x(fn, a, b, c, FLOAT, LDOUBLE)
#define	__TG_FN3(fn, a, b, c)	__TG_GFN3(fn, a, b, c, FLOAT, LDOUBLE)

#define	__TG_COMPLEX(a, fn)			\
	__TG_CHOOSE(__TG_IS_COMPLEX_P(a),	\
		    __TG_CFN1(c##fn, (a)),	\
		    __TG_FN1(fn, (a)))

#define	__TG_COMPLEX1(a, cfn, fn)		\
	__TG_CHOOSE(__TG_IS_COMPLEX_P(a),	\
		    __TG_CFN1(cfn, (a)),	\
		    __TG_FN1(fn, (a)))

#define	__TG_COMPLEX2(a, b, fn)			\
	__TG_CHOOSE(__TG_IS_COMPLEX_P(a)	\
		    || __TG_IS_COMPLEX_P(b),	\
		    __TG_CFN2(c##fn, (a), (b)),	\
		    __TG_FN2(fn, (a), (b)))

#define	acos(a)		__TG_COMPLEX((a), acos)
#define	asin(a)		__TG_COMPLEX((a), asin)
#define	atan(a)		__TG_COMPLEX((a), atan)
#define	acosh(a)	__TG_COMPLEX((a), acosh)
#define	asinh(a)	__TG_COMPLEX((a), asinh)
#define	atanh(a)	__TG_COMPLEX((a), atanh)
#define	cos(a)		__TG_COMPLEX((a), cos)
#define	sin(a)		__TG_COMPLEX((a), sin)
#define	tan(a)		__TG_COMPLEX((a), tan)
#define	cosh(a)		__TG_COMPLEX((a), cosh)
#define	sinh(a)		__TG_COMPLEX((a), sinh)
#define	tanh(a)		__TG_COMPLEX((a), tanh)
#define	exp(a)		__TG_COMPLEX((a), exp)
#define	log(a)		__TG_COMPLEX((a), log)
#define	pow(a,b)	__TG_COMPLEX2((a), (b), pow)
#define	sqrt(a)		__TG_COMPLEX((a), sqrt)
#define	fabs(a)		__TG_COMPLEX1((a), cabs, fabs)

#define	atan2(a,b)	__TG_FN2(atan2, (a), (b))
#define	cbrt(a)		__TG_FN1(cbrt, (a))
#define	ceil(a)		__TG_FN1(ceil, (a))
#define	copysign(a,b)	__TG_FN2(copysign, (a), (b))
#define	erf(a)		__TG_FN1(erf, (a))
#define	erfc(a)		__TG_FN1(erfc, (a))
#define	exp2(a)		__TG_FN1(exp2, (a))
#define	expm1(a)	__TG_FN1(expm1, (a))
#define	fdim(a,b)	__TG_FN2(fdim, (a), (b))
#define	floor(a)	__TG_FN1(floor, (a))
#define	fma(a,b,c)	__TG_FN3(fma, (a), (b), (c))
#define	fmax(a,b)	__TG_FN2(fmax, (a), (b))
#define	fmin(a,b)	__TG_FN2(fmin, (a), (b))
#define	fmod(a,b)	__TG_FN2(fmod, (a), (b))
#define	frexp(a,b)	__TG_FN1x(frexp, (a), (b))
#define	hypot(a,b)	__TG_FN2(hypot, (a), (b))
#define	ilogb(a)	__TG_FN1(ilogb, (a))
#define	ldexp(a,b)	__TG_FN1x(ldexp, (a), (b))
#define	lgamma(a)	__TG_FN1(lgamma, (a))
#define	llrint(a)	__TG_FN1(llrint, (a))
#define	llround(a)	__TG_FN1(llround, (a))
#define	log10(a)	__TG_FN1(log10, (a))
#define	log1p(a)	__TG_FN1(log1p, (a))
#define	log2(a)		__TG_FN1(log2, (a))
#define	logb(a)		__TG_FN1(logb, (a))
#define	lrint(a)	__TG_FN1(lrint, (a))
#define	lround(a)	__TG_FN1(lround, (a))
#define	nearbyint(a)	__TG_FN1(nearbyint, (a))
#define	nextafter(a,b)	__TG_FN2(nextafter, (a), (b))
#define	nexttoward(a,b)	__TG_FN2(nexttoward, (a), (b))
#define	remainder(a,b)	__TG_FN2(remainder, (a), (b))
#define	remquo(a,b,c)	__TG_FN2x(remquo, (a), (b), (c))
#define	rint(a)		__TG_FN1(rint, (a))
#define	round(a)	__TG_FN1(round, (a))
#define	scalbn(a,b)	__TG_FN1x(scalbn, (a), (b))
#define	scalb1n(a,b)	__TG_FN1x(scalb1n, (a), (b))
#define	tgamma(a)	__TG_FN1(tgamma, (a))
#define	trunc(a)	__TG_FN1(trunc, (a))

#define	carg(a)		__TG_CFN1(carg, (a))
#define	cimag(a)	__TG_CFN1(cimag, (a))
#define	conj(a)		__TG_CFN1(conj, (a))
#define	cproj(a)	__TG_CFN1(cproj, (a))
#define	creal(a)	__TG_CFN1(creal, (a))

#endif /* !_TGMATH_H_ */
