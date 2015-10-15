/* $NetBSD: unordtf2.c,v 1.2 2014/01/30 19:11:41 matt Exp $ */

/*
 * Written by Richard Earnshaw, 2003.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: unordtf2.c,v 1.2 2014/01/30 19:11:41 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#ifdef FLOAT128

flag __unordtf2(float128, float128);

flag
__unordtf2(float128 a, float128 b)
{
	/*
	 * The comparison is unordered if either input is a NaN.
	 * Test for this by comparing each operand with itself.
	 * We must perform both comparisons to correctly check for
	 * signalling NaNs.
	 */
	return 1 ^ (float128_eq(a, a) & float128_eq(b, b));
}

#endif /* FLOAT128 */
