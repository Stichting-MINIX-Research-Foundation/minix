/* w_hypotf.c -- float version of w_hypot.c.
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
__RCSID("$NetBSD: w_hypotf.c,v 1.7 2007/08/10 21:20:36 drochner Exp $");
#endif

/*
 * wrapper hypotf(x,y)
 */

#include "namespace.h"
#include "math.h"
#include "math_private.h"

#ifdef __weak_alias
__weak_alias(hypotf, _hypotf)
#endif

float
hypotf(float x, float y)	/* wrapper hypotf */
{
#ifdef _IEEE_LIBM
	return __ieee754_hypotf(x,y);
#else
	float z;
	z = __ieee754_hypotf(x,y);
	if(_LIB_VERSION == _IEEE_) return z;
	if((!finitef(z))&&finitef(x)&&finitef(y))
	    /* hypot overflow */
	    return (float)__kernel_standard((double)x,(double)y,104);
	else
	    return z;
#endif
}
