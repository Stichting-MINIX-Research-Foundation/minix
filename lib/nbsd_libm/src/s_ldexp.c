/* @(#)s_ldexp0.c 5.1 93/09/24 */
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
__RCSID("$NetBSD: s_ldexp.c,v 1.11 2010/04/23 19:17:07 drochner Exp $");
#endif

#include "namespace.h"
#include "math.h"
#include "math_private.h"
#include <errno.h>

double
ldexp(double value, int exp0)
{
	if(!finite(value)||value==0.0) return value;
	value = scalbn(value,exp0);
	if(!finite(value)||value==0.0) errno = ERANGE;
	return value;
}
