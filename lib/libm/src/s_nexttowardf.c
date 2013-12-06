/*	$NetBSD: s_nexttowardf.c,v 1.3 2013/02/09 23:14:44 christos Exp $	*/

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
#if 0
__FBSDID("$FreeBSD: src/lib/msun/src/s_nexttowardf.c,v 1.3 2011/02/10 07:38:38 das Exp $");
#else
__RCSID("$NetBSD: s_nexttowardf.c,v 1.3 2013/02/09 23:14:44 christos Exp $");
#endif

#include <string.h>
#include <float.h>
#include <machine/ieee.h>

#include "math.h"
#include "math_private.h"

#ifdef EXT_EXP_INFNAN
float
nexttowardf(float x, long double y)
{
	volatile float t;
	int32_t hx,ix;
	union ieee_ext_u uy;

	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;		/* |x| */

	memset(&uy, 0, sizeof(uy));
	uy.extu_ld = y;
	uy.extu_ext.ext_frach &= ~0x80000000;

	if((ix>0x7f800000) ||
	   (uy.extu_ext.ext_exp == EXT_EXP_INFNAN &&
	    (uy.extu_ext.ext_frach | uy.extu_ext.ext_fracl) != 0))
	   return x+y;	/* x or y is nan */
	if(x==y) return (float)y;		/* x=y, return y */
	if(ix==0) {				/* x == 0 */
	    SET_FLOAT_WORD(x,(uy.extu_ext.ext_sign<<31)|1);/* return +-minsubnormal */
	    t = x*x;
	    if(t==x) return t; else return x;	/* raise underflow flag */
	}
	if((hx >= 0) ^ (x < y))			/* x -= ulp */
	    hx -= 1;
	else					/* x += ulp */
	    hx += 1;
	ix = hx&0x7f800000;
	if(ix>=0x7f800000) return x+x;	/* overflow  */
	if(ix<0x00800000) {		/* underflow */
	    t = x*x;
	    if(t!=x) {		/* raise underflow flag */
	        SET_FLOAT_WORD(x,hx);
		return x;
	    }
	}
	SET_FLOAT_WORD(x,hx);
	return x;
}
#endif
