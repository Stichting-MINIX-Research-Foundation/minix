/* w_log10f.c -- float version of w_log10.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

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
__RCSID("$NetBSD: w_log2f.c,v 1.1 2005/07/21 16:58:39 christos Exp $");
#endif

/*
 * wrapper log2f(X)
 */

#include "math.h"
#include "math_private.h"


float
log2f(float x)		/* wrapper log2f */
{
#ifdef _IEEE_LIBM
	return __ieee754_log2f(x);
#else
	float z;
	z = __ieee754_log2f(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x)) return z;
	if(x<=(float)0.0) {
	    if(x==(float)0.0)
	        /* log2(0) */
	        return (float)__kernel_standard((double)x,(double)x,148);
	    else
	        /* log2(x<0) */
	        return (float)__kernel_standard((double)x,(double)x,149);
	} else
	    return z;
#endif
}
