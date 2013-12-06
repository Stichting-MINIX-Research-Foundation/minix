/*	$NetBSD: s_nexttoward.c,v 1.2 2013/08/21 13:03:56 martin Exp $	*/

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
__RCSID("$NetBSD: s_nexttoward.c,v 1.2 2013/08/21 13:03:56 martin Exp $");

/*
 * We assume that a long double has a 15-bit exponent.  On systems
 * where long double is the same as double, nexttoward() is an alias
 * for nextafter(), so we don't use this routine.
 */
#include <float.h>

#include <machine/ieee.h>
#include "math.h"
#include "math_private.h"

#if LDBL_MAX_EXP != 0x4000
#error "Unsupported long double format"
#endif

#ifdef LDBL_IMPLICIT_NBIT
#define	LDBL_NBIT	0
#endif

/*
 * The nexttoward() function is equivalent to nextafter() function,
 * except that the second parameter shall have type long double and
 * the functions shall return y converted to the type of the function
 * if x equals y.
 *
 * Special cases: XXX
 */
double
nexttoward(double x, long double y)
{
	union ieee_ext_u uy;
	volatile double t;
	int32_t hx, ix;
	uint32_t lx;

	EXTRACT_WORDS(hx, lx, x);
	ix = hx & 0x7fffffff;			/* |x| */
	uy.extu_ld = y;

	if (((ix >= 0x7ff00000) && ((ix - 0x7ff00000) | lx) != 0) ||
	    (uy.extu_exp == 0x7fff &&
		((uy.extu_frach & ~LDBL_NBIT) | uy.extu_fracl) != 0))
		return x+y;			/* x or y is nan */

	if (x == y)
		return (double)y;		/* x=y, return y */

	if (x == 0.0) {
		INSERT_WORDS(x, uy.extu_sign<<31, 1);	/* return +-minsubnormal */
		t = x*x;
		if (t == x)
			return t;
		else
			return x;		/* raise underflow flag */
	}

	if ((hx > 0.0) ^ (x < y)) {		/* x -= ulp */
		if (lx == 0) hx -= 1;
		lx -= 1;
	} else {				/* x += ulp */
		lx += 1;
		if (lx == 0) hx += 1;
	}
	ix = hx & 0x7ff00000;
	if (ix >= 0x7ff00000) return x+x;	/* overflow  */
	if (ix <  0x00100000) {			/* underflow */
		t = x*x;
		if (t != x) {			/* raise underflow flag */
			INSERT_WORDS(y, hx, lx);
			return y;
		}
	}
	INSERT_WORDS(x, hx, lx);

	return x;
}
