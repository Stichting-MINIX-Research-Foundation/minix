/*	$NetBSD: s_nextafterl.c,v 1.2 2010/09/17 20:39:39 christos Exp $	*/

/* @(#)s_nextafter.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: s_nextafterl.c,v 1.2 2010/09/17 20:39:39 christos Exp $");

#include <float.h>
#include <math.h>
#include <machine/ieee.h>

#ifdef EXT_EXP_INFNAN
#if LDBL_MAX_EXP != 0x4000
#error "Unsupported long double format"
#endif

/*
 * IEEE functions
 *      nextafterl(x,y)
 *      return the next machine floating-point number of x in the
 *      direction toward y.
 *   Special cases:
 *	If x == y, y shall be returned
 *	If x or y is NaN, a NaN shall be returned
 */
long double
nextafterl(long double x, long double y)
{
	volatile long double t;
	union ieee_ext_u ux, uy;

	ux.extu_ld = x;
	uy.extu_ld = y;

	if ((ux.extu_exp == EXT_EXP_NAN &&
		((ux.extu_frach &~ LDBL_NBIT)|ux.extu_fracl) != 0) ||
	    (uy.extu_exp == EXT_EXP_NAN &&
		((uy.extu_frach &~ LDBL_NBIT)|uy.extu_fracl) != 0))
		return x+y;			/* x or y is nan */

	if (x == y) return y;			/* x=y, return y */

	if (x == 0.0) {
		ux.extu_frach = 0;		/* return +-minsubnormal */
		ux.extu_fracl = 1;
		ux.extu_sign = uy.extu_sign;
		t = ux.extu_ld * ux.extu_ld;
		if (t == ux.extu_ld)
			return t;
		else
			return ux.extu_ld;	/* raise underflow flag */
	}

	if ((x>0.0) ^ (x<y)) {			/* x -= ulp */
		if (ux.extu_fracl == 0) {
			if ((ux.extu_frach & ~LDBL_NBIT) == 0)
				ux.extu_exp -= 1;
			ux.extu_frach = (ux.extu_frach - 1) |
					(ux.extu_frach & LDBL_NBIT);
		}
		ux.extu_fracl -= 1;
	} else {				/* x += ulp */
		ux.extu_fracl += 1;
		if (ux.extu_fracl == 0) {
			ux.extu_frach = (ux.extu_frach + 1) |
					(ux.extu_frach & LDBL_NBIT);
			if ((ux.extu_frach & ~LDBL_NBIT) == 0)
				ux.extu_exp += 1;
		}
	}

	if (ux.extu_exp == EXT_EXP_INF)
		return x+x;			/* overflow  */

	if (ux.extu_exp == 0) {			/* underflow */
		mask_nbit_l(ux);
		t = ux.extu_ld * ux.extu_ld;
		if (t != ux.extu_ld)		/* raise underflow flag */
			return ux.extu_ld;
	}

	return ux.extu_ld;
}
#endif
