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
 * From: @(#)s_floor.c 5.1 93/09/24
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: s_truncl.c,v 1.4 2013/11/13 12:58:11 joerg Exp $");
#if 0
__FBSDID("$FreeBSD: head/lib/msun/src/s_truncl.c 176280 2008-02-14 15:10:34Z bde $");
#endif

/*
 * truncl(x)
 * Return x rounded toward 0 to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to truncl(x).
 */
#include "namespace.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE

#ifdef __weak_alias
__weak_alias(truncl, _truncl)
#endif

#ifdef LDBL_IMPLICIT_NBIT
#define	MANH_SIZE	(EXT_FRACHBITS + 1)
#else
#define	MANH_SIZE	EXT_FRACHBITS
#endif

static const long double huge = 1.0e300;
static const float zero[] = { 0.0, -0.0 };

long double
truncl(long double x)
{
	union ieee_ext_u ux = { .extu_ld = x, };
	int e = ux.extu_exp - LDBL_MAX_EXP + 1;

	if (e < MANH_SIZE - 1) {
		if (e < 0) {			/* raise inexact if x != 0 */
			if (huge + x > 0.0)
				ux.extu_ld = zero[ux.extu_sign];
		} else {
			uint64_t m = ((1llu << MANH_SIZE) - 1) >> (e + 1);
			if (((ux.extu_frach & m) | ux.extu_fracl) == 0)
				return (x);	/* x is integral */
			if (huge + x > 0.0) {	/* raise inexact flag */
				ux.extu_frach &= ~m;
				ux.extu_fracl = 0;
			}
		}
	} else if (e < LDBL_MANT_DIG - 1) {
		uint64_t m = (uint64_t)-1 >> (64 - LDBL_MANT_DIG + e + 1);
		if ((ux.extu_fracl & m) == 0)
			return (x);	/* x is integral */
		if (huge + x > 0.0)		/* raise inexact flag */
			ux.extu_fracl &= ~m;
	}
	return (ux.extu_ld);
}

#endif /* __HAVE_LONG_DOUBLE */
