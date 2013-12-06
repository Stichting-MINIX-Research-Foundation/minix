/* @(#)w_fmod.c 5.1 93/09/24 */
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
__RCSID("$NetBSD: w_fmod.c,v 1.10 2013/11/12 16:48:39 joerg Exp $");
#endif

/*
 * wrapper fmod(x,y)
 */

#include "math.h"
#include "math_private.h"

#ifndef __HAVE_LONG_DOUBLE
__strong_alias(_fmodl, fmod)
__weak_alias(fmodl, fmod)
#endif

double
fmod(double x, double y)	/* wrapper fmod */
{
#ifdef _IEEE_LIBM
	return __ieee754_fmod(x,y);
#else
	double z;
	z = __ieee754_fmod(x,y);
	if(_LIB_VERSION == _IEEE_ ||isnan(y)||isnan(x)) return z;
	if(y==0.0) {
	        return __kernel_standard(x,y,27); /* fmod(x,0) */
	} else
	    return z;
#endif
}
