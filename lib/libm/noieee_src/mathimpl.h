/*	$NetBSD: mathimpl.h,v 1.10 2011/11/02 02:34:56 christos Exp $	*/
/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mathimpl.h	8.1 (Berkeley) 6/4/93
 */
#ifndef _NOIEEE_SRC_MATHIMPL_H_
#define _NOIEEE_SRC_MATHIMPL_H_

#include <sys/cdefs.h>
#include <math.h>
#include <stdint.h>

#if defined(__vax__) || defined(tahoe)

/* Deal with different ways to concatenate in cpp */
#define cat3(a,b,c)	a ## b ## c

/* Deal with vax/tahoe byte order issues */
#  ifdef __vax__
#    define	cat3t(a,b,c) cat3(a,b,c)
#  else
#    define	cat3t(a,b,c) cat3(a,c,b)
#  endif

#  define vccast(name) (cat3(__,name,x).d)

   /*
    * Define a constant to high precision on a Vax or Tahoe.
    *
    * Args are the name to define, the decimal floating point value,
    * four 16-bit chunks of the float value in hex
    * (because the vax and tahoe differ in float format!), the power
    * of 2 of the hex-float exponent, and the hex-float mantissa.
    * Most of these arguments are not used at compile time; they are
    * used in a post-check to make sure the constants were compiled
    * correctly.
    *
    * People who want to use the constant will have to do their own
    *     #define foo vccast(foo)
    * since CPP cannot do this for them from inside another macro (sigh).
    * We define "vccast" if this needs doing.
    */
#ifdef _LIBM_DECLARE
#  define vc(name, value, x1,x2,x3,x4, bexp, xval) \
    const union { uint32_t l[2]; double d; } cat3(__,name,x) = { \
	.l = { [0] = cat3t(0x,x1,x2), [1] = cat3t(0x,x3,x4) } };
#elif defined(_LIBM_STATIC)
#  define vc(name, value, x1,x2,x3,x4, bexp, xval) \
    static const union { uint32_t l[2]; double d; } cat3(__,name,x) = { \
	.l = { [0] = cat3t(0x,x1,x2), [1] = cat3t(0x,x3,x4) } };
#else
#  define vc(name, value, x1,x2,x3,x4, bexp, xval) \
	extern const union { uint32_t l[2]; double d; } cat3(__,name,x);
#endif
#  define ic(name, value, bexp, xval) 

#else	/* __vax__ or tahoe */

   /* Hooray, we have an IEEE machine */
#  undef vccast
#  define vc(name, value, x1,x2,x3,x4, bexp, xval) 

#ifdef _LIBM_DECLARE
#  define ic(name, value, bexp, xval) \
	const double __CONCAT(__,name) = value;
#elif _LIBM_STATIC
#  define ic(name, value, bexp, xval) \
	static const double __CONCAT(__,name) = value;
#else
#  define ic(name, value, bexp, xval) \
	extern const double __CONCAT(__,name);
#endif

#endif	/* defined(__vax__)||defined(tahoe) */

#ifdef __vax__
#include <machine/float.h>
#define _TINY	DBL_EPSILON
#define _TINYER	DBL_EPSILON
#define _HUGE	DBL_MAX
#else
#define _TINY	1e-300
#define _TINYER	1e-308
#define _HUGE	1e+300
#endif


/*
 * Functions internal to the math package, yet not static.
 */
extern double	__exp__E(double, double);
extern double	__log__L(double);
extern int	infnan(int);

struct Double {double a, b;};
double __exp__D(double, double);
struct Double __log__D(double);

#endif /* _NOIEEE_SRC_MATHIMPL_H_ */
