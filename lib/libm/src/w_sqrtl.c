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
__RCSID("$NetBSD: w_sqrtl.c,v 1.2 2013/11/20 11:39:00 joerg Exp $");

/*
 * wrapper sqrtl(x)
 */

#include "namespace.h"
#include "math.h"
#include "math_private.h"

#ifdef __HAVE_LONG_DOUBLE

__weak_alias(sqrtl, _sqrtl)

long double
sqrtl(long double x)		/* wrapper sqrtl */
{
#ifdef _IEEE_LIBM
	return __ieee754_sqrtl(x);
#else
	long double z;
	z = __ieee754_sqrtl(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(x<0.0) {
	    return __kernel_standard(x,x,226); /* sqrtl(negative) */
	} else
	    return z;
#endif
}

#endif /* __HAVE_LONG_DOUBLE */
