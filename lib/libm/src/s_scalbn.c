/* @(#)s_scalbn.c 5.1 93/09/24 */
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
#if defined(LIBM_SCCS) && !defined(lint)
__RCSID("$NetBSD: s_scalbn.c,v 1.18 2013/05/20 19:40:09 joerg Exp $");
#endif

/*
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */

#include "namespace.h"
#include "math.h"
#include "math_private.h"

#ifndef _LP64
__strong_alias(_scalbn, _scalbln)
#endif

#ifndef __HAVE_LONG_DOUBLE
__strong_alias(_scalbnl, _scalbn)
__strong_alias(_scalblnl, _scalbln)
__weak_alias(scalbnl, _scalbnl)
__weak_alias(scalblnl, _scalblnl)
#endif

#ifdef __weak_alias
__weak_alias(scalbn, _scalbn)
__weak_alias(scalbln, _scalbln)
__weak_alias(ldexp, _scalbn)
#endif

static const double
two54   =  0x1.0p54,	/* 0x43500000, 0x00000000 */
twom54  =  0x1.0p-54,	/* 0x3C900000, 0x00000000 */
huge   = 1.0e+300,
tiny   = 1.0e-300;

#ifdef _LP64
double
scalbn(double x, int n)
{
	return scalbln(x, n);
}
#endif

double
scalbln(double x, long n)
{
	int32_t k,hx,lx;
	EXTRACT_WORDS(hx,lx,x);
        k = ((uint32_t)hx&0x7ff00000)>>20;		/* extract exponent */
        if (k==0) {				/* 0 or subnormal x */
            if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
	    x *= two54;
	    GET_HIGH_WORD(hx,x);
	    k = (((uint32_t)hx&0x7ff00000)>>20) - 54;
            if (n< -50000) return tiny*x; 	/*underflow*/
	    }
        if (k==0x7ff) return x+x;		/* NaN or Inf */
        k = k+n;
        if (k >  0x7fe) return huge*copysign(huge,x); /* overflow  */
        if (k > 0) 				/* normal result */
	    {SET_HIGH_WORD(x,(hx&0x800fffff)|(k<<20)); return x;}
        if (k <= -54) {
            if (n > 50000) 	/* in case integer overflow in n+k */
		return huge*copysign(huge,x);	/*overflow*/
	    else return tiny*copysign(tiny,x); 	/*underflow*/
	}
        k += 54;				/* subnormal result */
	SET_HIGH_WORD(x,(hx&0x800fffff)|(k<<20));
        return x*twom54;
}
