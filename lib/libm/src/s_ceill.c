/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * From: @(#)s_ceil.c 5.1 93/09/24
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: s_ceill.c,v 1.1 2013/11/11 23:57:34 joerg Exp $");
#if 0
__FBSDID("$FreeBSD: head/lib/msun/src/s_ceill.c 176280 2008-02-14 15:10:34Z bde $");
#endif

/*
 * ceill(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to ceill(x).
 */
#include "namespace.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE

#ifdef __weak_alias
__weak_alias(ceill, _ceill)
#endif

#ifdef LDBL_IMPLICIT_NBIT
#define	MANH_SIZE	(EXT_FRACHBITS + 1)
#define	INC_MANH(ux, c)	do {					\
	uint64_t oi = ux.extu_frach;				\
	ux.extu_frach += (c);					\
	if (ux.extu_frach < oi)					\
		ux.extu_exp++;					\
} while (0)
#else
#define	MANH_SIZE	EXT_FRACHBITS
#define	INC_MANH(ux, c)	do {					\
	uint64_t oi = ux.extu_frach;				\
	ux.extu_frach += (c);					\
	if (ux.extu_frach < oi) {				\
		ux.extu_exp++;					\
		ux.extu_frach |= 1llu << (EXT_FRACHBITS - 1);	\
	}							\
} while (0)
#endif

static const long double huge = 1.0e300;

long double
ceill(long double x)
{
	union ieee_ext_u ux = { .extu_ld = x, };
	int e = ux.extu_exp - LDBL_MAX_EXP + 1;

	if (e < MANH_SIZE - 1) {
		if (e < 0) {			/* raise inexact if x != 0 */
			if (huge + x > 0.0)
				if (ux.extu_exp > 0 ||
				    (ux.extu_frach | ux.extu_fracl) != 0)
					ux.extu_ld = ux.extu_sign ? -0.0 : 1.0;
		} else {
			uint64_t m = ((1llu << MANH_SIZE) - 1) >> (e + 1);
			if (((ux.extu_frach & m) | ux.extu_fracl) == 0)
				return (x);	/* x is integral */
			if (!ux.extu_sign) {
#ifdef LDBL_IMPLICIT_NBIT
				if (e == 0)
					ux.extu_exp++;
				else
#endif
				INC_MANH(ux, 1llu << (MANH_SIZE - e - 1));
			}
			if (huge + x > 0.0) {	/* raise inexact flag */
				ux.extu_frach &= ~m;
				ux.extu_fracl = 0;
			}
		}
	} else if (e < LDBL_MANT_DIG - 1) {
		uint64_t m = (uint64_t)-1 >> (64 - LDBL_MANT_DIG + e + 1);
		if ((ux.extu_fracl & m) == 0)
			return (x);	/* x is integral */
		if (!ux.extu_sign) {
			if (e == MANH_SIZE - 1)
				INC_MANH(ux, 1);
			else {
				uint64_t o = ux.extu_fracl;
				ux.extu_fracl += 1llu << (LDBL_MANT_DIG - e - 1);
				if (ux.extu_fracl < o)	/* got a carry */
					INC_MANH(ux, 1);
			}
		}
		if (huge + x > 0.0)		/* raise inexact flag */
			ux.extu_fracl &= ~m;
	}
	return (ux.extu_ld);
}

#endif /* __HAVE_LONG_DOUBLE */
