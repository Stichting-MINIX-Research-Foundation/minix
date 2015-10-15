/*-
 * Copyright (c) 2007 David Schultz <das@FreeBSD.ORG>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Derived from s_modf.c, which has the following Copyright:
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * $FreeBSD: head/lib/msun/src/s_modfl.c 165855 2007-01-07 07:54:21Z das $
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: s_modfl.c,v 1.1 2014/06/16 12:54:43 joerg Exp $");

#include "namespace.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE

#ifdef __weak_alias
__weak_alias(modfl, _modfl)
#endif

#if LDBL_MANL_SIZE > 32
#define	MASK	((uint64_t)-1)
#else
#define	MASK	((uint32_t)-1)
#endif
/* Return the last n bits of a word, representing the fractional part. */
#define	GETFRAC(bits, n)	((bits) & ~(MASK << (n)))
/* The number of fraction bits in manh, not counting the integer bit */
#define	HIBITS	(LDBL_MANT_DIG - EXT_FRACHBITS)

static const long double zero[] = { 0.0L, -0.0L };

long double
modfl(long double x, long double *iptr)
{
	union ieee_ext_u ux = { .extu_ld = x, };
	int e = ux.extu_exp - LDBL_MAX_EXP + 1;

	if (e < HIBITS) {			/* Integer part is in manh. */
		if (e < 0) {			/* |x|<1 */
			*iptr = zero[ux.extu_sign];
			return (x);
		} else {
			if ((GETFRAC(ux.extu_frach, HIBITS - 1 - e) |
			     ux.extu_fracl) == 0) {	/* X is an integer. */
				*iptr = x;
				return (zero[ux.extu_sign]);
			} else {
				/* Clear all but the top e+1 bits. */
				ux.extu_frach >>= HIBITS - 1 - e;
				ux.extu_frach <<= HIBITS - 1 - e;
				ux.extu_fracl = 0;
				*iptr = ux.extu_ld;
				return (x - ux.extu_ld);
			}
		}
	} else if (e >= LDBL_MANT_DIG - 1) {	/* x has no fraction part. */
		*iptr = x;
		if (x != x)			/* Handle NaNs. */
			return (x);
		return (zero[ux.extu_sign]);
	} else {				/* Fraction part is in manl. */
		if (GETFRAC(ux.extu_fracl, LDBL_MANT_DIG - 1 - e) == 0) {
			/* x is integral. */
			*iptr = x;
			return (zero[ux.extu_sign]);
		} else {
			/* Clear all but the top e+1 bits. */
			ux.extu_fracl >>= LDBL_MANT_DIG - 1 - e;
			ux.extu_fracl <<= LDBL_MANT_DIG - 1 - e;
			*iptr = ux.extu_ld;
			return (x - ux.extu_ld);
		}
	}
}
#endif /* __HAVE_LONG_DOUBLE */
