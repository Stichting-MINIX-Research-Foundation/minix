/* s_ldexp0f.c -- float version of s_ldexp0.c.
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
__RCSID("$NetBSD: s_ldexpf.c,v 1.8 2010/04/23 19:17:07 drochner Exp $");
#endif

#include "namespace.h"
#include "math.h"
#include "math_private.h"
#include <errno.h>

float
ldexpf(float value, int exp0)
{
	if(!finitef(value)||value==(float)0.0) return value;
	value = scalbnf(value,exp0);
	if(!finitef(value)||value==(float)0.0) errno = ERANGE;
	return value;
}
