/* @(#)w_cosh.c 5.1 93/09/24 */
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
__RCSID("$NetBSD: w_cosh.c,v 1.10 2007/08/20 16:01:39 drochner Exp $");
#endif

/*
 * wrapper cosh(x)
 */

#include "namespace.h"
#include "math.h"
#include "math_private.h"

#ifdef __weak_alias
__weak_alias(cosh, _cosh)
#endif

double
cosh(double x)		/* wrapper cosh */
{
#ifdef _IEEE_LIBM
	return __ieee754_cosh(x);
#else
	double z;
	z = __ieee754_cosh(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(fabs(x)>7.10475860073943863426e+02) {
	        return __kernel_standard(x,x,5); /* cosh overflow */
	} else
	    return z;
#endif
}
