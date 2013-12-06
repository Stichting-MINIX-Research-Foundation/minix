/* @(#)w_sqrt.c 5.1 93/09/24 */
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
__RCSID("$NetBSD: w_sqrt.c,v 1.10 2013/11/19 19:24:34 joerg Exp $");
#endif

/*
 * wrapper sqrt(x)
 */

#include "namespace.h"
#include "math.h"
#include "math_private.h"

#ifndef __HAVE_LONG_DOUBLE
__strong_alias(_sqrtl, sqrt)
__weak_alias(sqrtl, _sqrtl)
#endif

double
sqrt(double x)		/* wrapper sqrt */
{
#ifdef _IEEE_LIBM
	return __ieee754_sqrt(x);
#else
	double z;
	z = __ieee754_sqrt(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(x<0.0) {
	    return __kernel_standard(x,x,26); /* sqrt(negative) */
	} else
	    return z;
#endif
}
