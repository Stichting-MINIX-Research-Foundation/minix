/* @(#)w_sinh.c 5.1 93/09/24 */
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
__RCSID("$NetBSD: w_sinh.c,v 1.10 2007/08/20 16:01:40 drochner Exp $");
#endif

/*
 * wrapper sinh(x)
 */

#include "namespace.h"
#include "math.h"
#include "math_private.h"

#ifdef __weak_alias
__weak_alias(sinh, _sinh)
#endif

double
sinh(double x)		/* wrapper sinh */
{
#ifdef _IEEE_LIBM
	return __ieee754_sinh(x);
#else
	double z;
	z = __ieee754_sinh(x);
	if(_LIB_VERSION == _IEEE_) return z;
	if(!finite(z)&&finite(x)) {
	    return __kernel_standard(x,x,25); /* sinh overflow */
	} else
	    return z;
#endif
}
